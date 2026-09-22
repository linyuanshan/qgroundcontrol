#!/usr/bin/env python3

"""Map ArduRover SITL No-Go incursions to a persisted Marine canonical path."""

from __future__ import annotations

import argparse
import csv
import json
import math
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

EARTH_RADIUS_M = 6_378_137.0
BACKEND_TOLERANCE_M = 0.002


@dataclass
class Point:
    x: float
    y: float


@dataclass
class Incident:
    timestamp_ms: int
    mission_sequence: int
    latitude_deg: float
    longitude_deg: float
    penetration_depth_m: float
    nearest_segment_index: int
    nearest_waypoint_index: int
    nearest_leg_role: str
    distance_to_planned_segment_m: float
    waypoint_before: list[float] | None
    waypoint: list[float]
    waypoint_after: list[float] | None
    incoming_heading_deg: float | None
    outgoing_heading_deg: float | None
    turn_angle_deg: float | None
    incoming_length_m: float | None
    outgoing_length_m: float | None
    structure_classification: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--trajectory", type=Path, required=True)
    parser.add_argument("--output-json", type=Path, required=True)
    parser.add_argument("--output-csv", type=Path, required=True)
    parser.add_argument("--safety-margin", type=float, default=1.0)
    return parser.parse_args()


def to_local(
    latitude: float, longitude: float, reference_latitude: float, reference_longitude: float
) -> Point:
    latitude_radians = math.radians(reference_latitude)
    return Point(
        math.radians(longitude - reference_longitude) * EARTH_RADIUS_M * math.cos(latitude_radians),
        math.radians(latitude - reference_latitude) * EARTH_RADIUS_M,
    )


def point_distance(first: Point, second: Point) -> float:
    return math.hypot(second.x - first.x, second.y - first.y)


def point_to_segment_distance(point: Point, first: Point, second: Point) -> float:
    dx = second.x - first.x
    dy = second.y - first.y
    length_squared = dx * dx + dy * dy
    if length_squared == 0.0:
        return point_distance(point, first)
    projection = max(
        0.0, min(1.0, ((point.x - first.x) * dx + (point.y - first.y) * dy) / length_squared)
    )
    projected = Point(first.x + projection * dx, first.y + projection * dy)
    return point_distance(point, projected)


def point_to_polygon_boundary_distance(point: Point, polygon: list[Point]) -> float:
    return min(
        point_to_segment_distance(point, polygon[index - 1], polygon[index])
        for index in range(len(polygon))
    )


def point_strictly_inside(point: Point, polygon: list[Point]) -> bool:
    inside = False
    previous = polygon[-1]
    for current in polygon:
        if (current.y > point.y) != (previous.y > point.y):
            crossing_x = (previous.x - current.x) * (point.y - current.y) / (
                previous.y - current.y
            ) + current.x
            if point.x < crossing_x:
                inside = not inside
        previous = current
    return inside and point_to_polygon_boundary_distance(point, polygon) > 1e-9


def heading(first: Point, second: Point) -> float:
    return math.degrees(math.atan2(second.x - first.x, second.y - first.y)) % 360.0


def turn_angle(incoming: float, outgoing: float) -> float:
    return abs((outgoing - incoming + 180.0) % 360.0 - 180.0)


def geo_pair(point: list[Any] | dict[str, Any]) -> tuple[float, float]:
    if isinstance(point, dict):
        return float(point["lat"]), float(point["lon"])
    return float(point[0]), float(point[1])


def find_complex_item(plan: dict[str, Any]) -> dict[str, Any]:
    for item in plan["mission"]["items"]:
        if item.get("complexItemType") == "coverageInspection":
            return item
    raise ValueError("coverageInspection complex item not found")


def classify_structure(
    segment_first: Point, segment_second: Point, role: str, inflated_no_go: list[Point]
) -> str:
    distance = min(
        point_to_polygon_boundary_distance(segment_first, inflated_no_go),
        point_to_polygon_boundary_distance(segment_second, inflated_no_go),
    )
    if distance <= BACKEND_TOLERANCE_M:
        return "No-Go boundary-support candidate"
    if role == "coverage":
        return "cell sweep"
    return "Unknown"


def write_csv(path: Path, incidents: list[Incident]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = (
        list(asdict(incidents[0]).keys())
        if incidents
        else [field.name for field in Incident.__dataclass_fields__.values()]
    )
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for incident in incidents:
            row = asdict(incident)
            for key in ("waypoint_before", "waypoint", "waypoint_after"):
                row[key] = (
                    json.dumps(row[key], separators=(",", ":")) if row[key] is not None else ""
                )
            writer.writerow(row)


def main() -> int:
    args = parse_args()
    plan = json.loads(args.plan.read_text(encoding="utf-8"))
    complex_item = find_complex_item(plan)
    path_geo = [geo_pair(point) for point in complex_item["generatedPath"]]
    roles = [str(role) for role in complex_item["legRoles"]]
    if len(roles) + 1 != len(path_geo):
        raise ValueError("generatedPath/legRoles size mismatch")

    task = plan["marine"]["tasks"][0]
    no_go_geo = [geo_pair(point) for point in task["region"]["noGoRegions"][0]]
    reference_latitude, reference_longitude = path_geo[0]
    path_local = [
        to_local(lat, lon, reference_latitude, reference_longitude) for lat, lon in path_geo
    ]
    no_go_local = [
        to_local(lat, lon, reference_latitude, reference_longitude) for lat, lon in no_go_geo
    ]

    minimum_x = min(point.x for point in no_go_local) - args.safety_margin
    maximum_x = max(point.x for point in no_go_local) + args.safety_margin
    minimum_y = min(point.y for point in no_go_local) - args.safety_margin
    maximum_y = max(point.y for point in no_go_local) + args.safety_margin
    inflated_no_go = [
        Point(minimum_x, minimum_y),
        Point(maximum_x, minimum_y),
        Point(maximum_x, maximum_y),
        Point(minimum_x, maximum_y),
    ]

    incidents: list[Incident] = []
    with args.trajectory.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            latitude = float(row["latitude_deg"])
            longitude = float(row["longitude_deg"])
            actual = to_local(latitude, longitude, reference_latitude, reference_longitude)
            if not point_strictly_inside(actual, no_go_local):
                continue

            segment_distances = [
                point_to_segment_distance(actual, path_local[index], path_local[index + 1])
                for index in range(len(path_local) - 1)
            ]
            segment_index = min(range(len(segment_distances)), key=segment_distances.__getitem__)
            waypoint_index = min(
                range(len(path_local)), key=lambda index: point_distance(actual, path_local[index])
            )
            incoming_heading = None
            outgoing_heading = None
            incoming_length = None
            outgoing_length = None
            angle = None
            if waypoint_index > 0:
                incoming_heading = heading(
                    path_local[waypoint_index - 1], path_local[waypoint_index]
                )
                incoming_length = point_distance(
                    path_local[waypoint_index - 1], path_local[waypoint_index]
                )
            if waypoint_index + 1 < len(path_local):
                outgoing_heading = heading(
                    path_local[waypoint_index], path_local[waypoint_index + 1]
                )
                outgoing_length = point_distance(
                    path_local[waypoint_index], path_local[waypoint_index + 1]
                )
            if incoming_heading is not None and outgoing_heading is not None:
                angle = turn_angle(incoming_heading, outgoing_heading)

            role = roles[segment_index]
            incidents.append(
                Incident(
                    timestamp_ms=int(row["timestamp_ms"]),
                    mission_sequence=int(row["mission_sequence"]),
                    latitude_deg=latitude,
                    longitude_deg=longitude,
                    penetration_depth_m=point_to_polygon_boundary_distance(actual, no_go_local),
                    nearest_segment_index=segment_index,
                    nearest_waypoint_index=waypoint_index,
                    nearest_leg_role=role,
                    distance_to_planned_segment_m=segment_distances[segment_index],
                    waypoint_before=(
                        [path_geo[waypoint_index - 1][0], path_geo[waypoint_index - 1][1]]
                        if waypoint_index > 0
                        else None
                    ),
                    waypoint=[path_geo[waypoint_index][0], path_geo[waypoint_index][1]],
                    waypoint_after=(
                        [path_geo[waypoint_index + 1][0], path_geo[waypoint_index + 1][1]]
                        if waypoint_index + 1 < len(path_geo)
                        else None
                    ),
                    incoming_heading_deg=incoming_heading,
                    outgoing_heading_deg=outgoing_heading,
                    turn_angle_deg=angle,
                    incoming_length_m=incoming_length,
                    outgoing_length_m=outgoing_length,
                    structure_classification=classify_structure(
                        path_local[segment_index],
                        path_local[segment_index + 1],
                        role,
                        inflated_no_go,
                    ),
                )
            )

    summary = {
        "sourcePlan": str(args.plan),
        "sourceTrajectory": str(args.trajectory),
        "insideSampleCount": len(incidents),
        "maximumPenetrationM": max(
            (incident.penetration_depth_m for incident in incidents), default=0.0
        ),
        "missionSequences": sorted({incident.mission_sequence for incident in incidents}),
        "nearestLegRoleCounts": {
            role: sum(incident.nearest_leg_role == role for incident in incidents)
            for role in sorted(set(roles))
        },
        "structureCounts": {
            classification: sum(
                incident.structure_classification == classification for incident in incidents
            )
            for classification in sorted(
                {incident.structure_classification for incident in incidents}
            )
        },
        "incidents": [asdict(incident) for incident in incidents],
    }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    write_csv(args.output_csv, incidents)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
