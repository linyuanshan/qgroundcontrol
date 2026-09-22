#!/usr/bin/env python3

"""Quantify backend-boundary density in persisted P2 canonical paths."""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any

WGS84_SEMI_MAJOR_AXIS_M = 6_378_137.0
WGS84_ECCENTRICITY_SQUARED = 6.69437999014e-3
BOUNDARY_TOLERANCE_M = 0.01
LENGTH_THRESHOLDS_M = (0.01, 0.05, 0.10, 0.50)


@dataclass(frozen=True)
class Point:
    x: float
    y: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--plan",
        action="append",
        required=True,
        metavar="NAME=PATH",
        help="Persisted plan and report name; may be repeated",
    )
    parser.add_argument("--output-json", type=Path, required=True)
    return parser.parse_args()


def parse_plan_argument(value: str) -> tuple[str, Path]:
    name, separator, path = value.partition("=")
    if not separator or not name or not path:
        raise ValueError(f"invalid --plan value: {value!r}")
    return name, Path(path)


def geo_pair(point: list[Any] | dict[str, Any]) -> tuple[float, float]:
    if isinstance(point, dict):
        return float(point["lat"]), float(point["lon"])
    return float(point[0]), float(point[1])


def to_local(latitude: float, longitude: float, reference: tuple[float, float]) -> Point:
    reference_latitude, reference_longitude = reference
    latitude_radians = math.radians(reference_latitude)
    denominator = math.sqrt(
        1.0 - WGS84_ECCENTRICITY_SQUARED * math.sin(latitude_radians) ** 2
    )
    prime_vertical_radius = WGS84_SEMI_MAJOR_AXIS_M / denominator
    meridional_radius = (
        WGS84_SEMI_MAJOR_AXIS_M
        * (1.0 - WGS84_ECCENTRICITY_SQUARED)
        / denominator**3
    )
    return Point(
        math.radians(longitude - reference_longitude)
        * prime_vertical_radius
        * math.cos(latitude_radians),
        math.radians(latitude - reference_latitude) * meridional_radius,
    )


def distance(first: Point, second: Point) -> float:
    return math.hypot(second.x - first.x, second.y - first.y)


def point_to_segment_distance(point: Point, first: Point, second: Point) -> float:
    dx = second.x - first.x
    dy = second.y - first.y
    length_squared = dx * dx + dy * dy
    if length_squared == 0.0:
        return distance(point, first)
    projection = max(
        0.0,
        min(
            1.0,
            ((point.x - first.x) * dx + (point.y - first.y) * dy) / length_squared,
        ),
    )
    return math.hypot(
        point.x - (first.x + projection * dx),
        point.y - (first.y + projection * dy),
    )


def point_to_boundary_distance(point: Point, polygon: list[Point]) -> float:
    return min(
        point_to_segment_distance(point, polygon[index - 1], polygon[index])
        for index in range(len(polygon))
    )


def quantile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def turn_angle(before: Point, point: Point, after: Point) -> float | None:
    incoming = Point(point.x - before.x, point.y - before.y)
    outgoing = Point(after.x - point.x, after.y - point.y)
    incoming_length = math.hypot(incoming.x, incoming.y)
    outgoing_length = math.hypot(outgoing.x, outgoing.y)
    if incoming_length == 0.0 or outgoing_length == 0.0:
        return None
    cosine = max(
        -1.0,
        min(
            1.0,
            (incoming.x * outgoing.x + incoming.y * outgoing.y)
            / (incoming_length * outgoing_length),
        ),
    )
    return math.degrees(math.acos(cosine))


def distribution(values: list[float]) -> dict[str, float | int | None]:
    return {
        "count": len(values),
        "minimum": min(values) if values else None,
        "p05": quantile(values, 0.05),
        "median": quantile(values, 0.50),
        "p95": quantile(values, 0.95),
        "maximum": max(values) if values else None,
    }


def length_statistics(lengths: list[float]) -> dict[str, Any]:
    result: dict[str, Any] = distribution(lengths)
    result["total"] = sum(lengths)
    result["thresholdCounts"] = {
        f"lt{threshold:.2f}M": sum(length < threshold for length in lengths)
        for threshold in LENGTH_THRESHOLDS_M
    }
    return result


def find_complex_item(plan: dict[str, Any]) -> dict[str, Any]:
    for item in plan["mission"]["items"]:
        if item.get("complexItemType") == "coverageInspection":
            return item
    raise ValueError("coverageInspection complex item not found")


def classify_leg(
    first: Point,
    second: Point,
    role: str,
    outer: list[Point],
    no_go_regions: list[list[Point]],
    safety_margin: float,
) -> str:
    if role != "coverage":
        return "other"
    for no_go in no_go_regions:
        if all(
            abs(point_to_boundary_distance(point, no_go) - safety_margin)
            <= BOUNDARY_TOLERANCE_M
            for point in (first, second)
        ):
            return "noGoBoundarySupport"
    if all(
        abs(point_to_boundary_distance(point, outer) - safety_margin)
        <= BOUNDARY_TOLERANCE_M
        for point in (first, second)
    ):
        return "outerBoundarySupport"
    return "cellCoverage"


def point_key(point: Point) -> tuple[int, int]:
    return round(point.x * 1_000_000), round(point.y * 1_000_000)


def edge_key(first: Point, second: Point) -> tuple[tuple[int, int], tuple[int, int]]:
    keys = sorted((point_key(first), point_key(second)))
    return keys[0], keys[1]


def is_round_arc_tessellation_point(point: Point, no_go: list[Point], margin: float) -> bool:
    minimum_x = min(vertex.x for vertex in no_go)
    maximum_x = max(vertex.x for vertex in no_go)
    minimum_y = min(vertex.y for vertex in no_go)
    maximum_y = max(vertex.y for vertex in no_go)
    outside_x = point.x < minimum_x - BOUNDARY_TOLERANCE_M or point.x > maximum_x + BOUNDARY_TOLERANCE_M
    outside_y = point.y < minimum_y - BOUNDARY_TOLERANCE_M or point.y > maximum_y + BOUNDARY_TOLERANCE_M
    if not outside_x or not outside_y:
        return False
    corner = Point(
        minimum_x if point.x < minimum_x else maximum_x,
        minimum_y if point.y < minimum_y else maximum_y,
    )
    return abs(distance(point, corner) - margin) <= BOUNDARY_TOLERANCE_M


def analyze_plan(name: str, path: Path) -> dict[str, Any]:
    plan = json.loads(path.read_text(encoding="utf-8"))
    item = find_complex_item(plan)
    task = plan["marine"]["tasks"][0]
    path_geo = [geo_pair(point) for point in item["generatedPath"]]
    roles = [str(role) for role in item["legRoles"]]
    if len(path_geo) != len(roles) + 1:
        raise ValueError(f"{name}: generatedPath/legRoles size mismatch")
    reference = path_geo[0]
    points = [to_local(latitude, longitude, reference) for latitude, longitude in path_geo]
    outer = [
        to_local(latitude, longitude, reference)
        for latitude, longitude in map(geo_pair, task["region"]["outerBoundary"])
    ]
    no_go_regions = [
        [
            to_local(latitude, longitude, reference)
            for latitude, longitude in map(geo_pair, polygon)
        ]
        for polygon in task["region"]["noGoRegions"]
    ]
    safety_margin = float(task["coverage"]["safetyMarginM"])

    lengths = [distance(points[index], points[index + 1]) for index in range(len(roles))]
    classifications = [
        classify_leg(
            points[index],
            points[index + 1],
            roles[index],
            outer,
            no_go_regions,
            safety_margin,
        )
        for index in range(len(roles))
    ]
    angles = [turn_angle(points[index - 1], points[index], points[index + 1]) for index in range(1, len(points) - 1)]

    def category_statistics(category: str, labels: list[str]) -> dict[str, Any]:
        indices = [index for index, label in enumerate(labels) if label == category]
        point_indices = sorted({endpoint for index in indices for endpoint in (index, index + 1)})
        category_angles = [
            angles[index - 1]
            for index in range(1, len(points) - 1)
            if labels[index - 1] == category and labels[index] == category and angles[index - 1] is not None
        ]
        return {
            "pointCount": len(point_indices),
            "legCount": len(indices),
            "lengthM": length_statistics([lengths[index] for index in indices]),
            "turnCount": sum(angle > 1e-9 for angle in category_angles),
            "turnAnglesDeg": distribution(category_angles),
        }

    role_stats = {role: category_statistics(role, roles) for role in ("coverage", "transit")}
    structure_names = ("noGoBoundarySupport", "outerBoundarySupport", "cellCoverage", "other")
    structure_stats = {
        structure: category_statistics(structure, classifications) for structure in structure_names
    }

    no_go_indices = [
        index for index, classification in enumerate(classifications) if classification == "noGoBoundarySupport"
    ]
    unique_no_go_edges: dict[tuple[tuple[int, int], tuple[int, int]], float] = {}
    boundary_point_indices: set[int] = set()
    for index in no_go_indices:
        unique_no_go_edges.setdefault(edge_key(points[index], points[index + 1]), lengths[index])
        boundary_point_indices.update((index, index + 1))
    boundary_points = {point_key(points[index]) for index in boundary_point_indices}
    unique_boundary_lengths = list(unique_no_go_edges.values())
    tessellation_waypoint_indices = {
        index
        for index in boundary_point_indices
        if any(
            is_round_arc_tessellation_point(points[index], no_go, safety_margin)
            for no_go in no_go_regions
        )
    }
    tessellation_geometry_points = {point_key(points[index]) for index in tessellation_waypoint_indices}

    all_angles = [angle for angle in angles if angle is not None]
    return {
        "scenario": name,
        "sourcePlan": str(path),
        "pointCount": len(points),
        "legCount": len(roles),
        "lengthM": length_statistics(lengths),
        "turnCount": sum(angle > 1e-9 for angle in all_angles),
        "turnAnglesDeg": distribution(all_angles),
        "roles": role_stats,
        "structures": structure_stats,
        "noGoBoundary": {
            "holeBoundaryVertexCount": len(boundary_points),
            "holeBoundaryPerimeterM": sum(unique_boundary_lengths),
            "meanWaypointSpacingM": (
                sum(unique_boundary_lengths) / len(unique_boundary_lengths)
                if unique_boundary_lengths
                else None
            ),
            "minimumWaypointSpacingM": min(unique_boundary_lengths) if unique_boundary_lengths else None,
            "medianWaypointSpacingM": quantile(unique_boundary_lengths, 0.50),
            "roundTessellationMissionWaypointCount": len(tessellation_waypoint_indices),
            "roundTessellationUniqueGeometryPointCount": len(tessellation_geometry_points),
        },
    }


def main() -> int:
    args = parse_args()
    reports = [analyze_plan(*parse_plan_argument(value)) for value in args.plan]
    output = {"boundaryToleranceM": BOUNDARY_TOLERANCE_M, "scenarios": reports}
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
