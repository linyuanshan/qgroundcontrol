"""FC3 独立 evidence rebinding. 复用 R1 场景执行, 不修改冻结 fixture 或校准。"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path

import v05_10_r1 as r1

ROOT = r1.ROOT
CLOSURE = ROOT / "build/v05-10-r1/final-closure"
HISTORICAL_LOCK = ROOT / "build/v05-10-r1/fixture-lock-final.json"
HISTORICAL_LOCK_SHA = "b45de1f0d00de2e9c1f84ad71ec5b8b43b43917215ce06f94c78df0d85df836d"
HISTORICAL_ACCEPTANCE = ROOT / "build/v05-10-r1/acceptance"
SCENARIOS = ("M00", "M04", "M05", "M08", "M09", "STALE")
CANONICAL = {
    "M00": "M00|C=rect(0,0,90,90)|N=C|O=[]|swath=67.5|H=0|P=0|E=4.5|req=Standard|sweep=Manual90|planner=Auto",
    "M04": "M04|C=rect(0,0,90,90)|N=rect(-5.5,-5.5,95.5,95.5)|O=[]|swath=67.5|H=0|P=8|E=4.5|req=Strict|sweep=Manual90|planner=Auto",
}
FIXTURE_SHA = {
    "M00": "b2c40fb313785b91467859e1342b409e4f94679e7dcd47f78451ba06e3701526",
    "M04": "e3acd31f0445498dfb722f1ed84c630d8b09c18c1e4f5b6776c213b42dd4d141",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_new(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def run_stage(output: Path, phase: str) -> str:
    require(phase in ("preparation", "acceptance"), "Unknown FC3 phase")
    identity = os.path.normcase(str(output.resolve())).encode("utf-8")
    run_id = hashlib.sha256(identity).hexdigest()[:20]
    return f"fc3-{phase}-{run_id}"


def validate_paths(output: Path, lock: Path) -> None:
    for path in (output, lock):
        require(path.is_relative_to(CLOSURE), "FC3 evidence/lock must be under final-closure")
        require(
            not path.is_relative_to(CLOSURE / "final-candidate"),
            "Do not write into the historical FC2 frozen candidate",
        )
    require(output != CLOSURE, "Use an independent FC3 output directory")
    require(lock.name == "fc3-fixture-lock.json", "Use an independent fc3-fixture-lock.json")


def validate_frozen_inputs() -> None:
    require(r1.sha(HISTORICAL_LOCK) == HISTORICAL_LOCK_SHA, "Historical fixture lock hash mismatch")
    historical = load(HISTORICAL_LOCK)
    require(
        r1.sha(r1.PROTOCOL) == historical["protocol_sha256"], "Frozen calibration protocol changed"
    )
    require(
        r1.sha(Path(r1.__file__)) == historical["source_files"]["tools/simulation/v05_10_r1.py"],
        "Historical execution runner changed",
    )
    for name in (
        "custom/test/Marine/MarineSITLValidationTest.cc",
        "custom/test/Marine/V05ExecutionFixtures.h",
    ):
        require(
            r1.sha(ROOT / name) == historical["source_files"][name], f"Frozen input changed: {name}"
        )


def check_identity(source_head: str, binary: Path, binary_sha: str) -> None:
    require(bool(source_head), "Explicit source HEAD required")
    require(r1.command(["git", "rev-parse", "HEAD"]) == source_head, "Source HEAD mismatch")
    require(
        not r1.command(["git", "status", "--porcelain=v1"]), "Clean source worktree/index required"
    )
    require(r1.sha(binary) == binary_sha, "Candidate binary SHA mismatch")


def validate_fixture(directory: Path, scenario: str, source_head: str, binary_sha: str) -> dict:
    evidence = load(directory / "scenario.json")
    require(evidence["scenario"] == scenario, "Scenario mismatch")
    require(evidence["canonical"] == CANONICAL[scenario], "Frozen canonical fixture changed")
    require(evidence["canonicalSha256"] == FIXTURE_SHA[scenario], "Frozen fixture SHA changed")
    require(evidence["sourceHead"] == source_head, "Evidence source HEAD mismatch")
    require(evidence["binarySha256"] == binary_sha, "Evidence binary SHA mismatch")
    require(evidence["taskId"] == f"V05-10-R1-{scenario}", "Task ID changed")
    for name, key in (("task.json", "taskSha256"), ("artifact.json", "artifactSha256")):
        require(r1.sha(directory / name) == evidence[key], f"{name} hash mismatch")
        require(
            load(directory / name) == load(HISTORICAL_ACCEPTANCE / scenario / name),
            f"{name} differs from the frozen R1 Task/Artifact semantics",
        )
    require(
        evidence["planningStatus"] == 0 and not evidence["resultStale"],
        "Planning not current Success",
    )
    require(evidence["readiness"] == (1 if scenario == "M00" else 2), "Readiness changed")
    require(evidence["tier"] == (0 if scenario == "M00" else 1), "Safety tier changed")
    require(evidence["uploadAllowed"] is True, "Upload must remain allowed")
    return evidence


def validate_lock(lock: dict, source_head: str, binary_sha: str) -> None:
    require(lock["source_head"] == source_head, "Lock source HEAD mismatch")
    require(lock["binary_sha256"] == binary_sha, "Lock binary SHA mismatch")
    require(lock["protocol_sha256"] == r1.sha(r1.PROTOCOL), "Protocol changed")
    require(
        lock["image_id"] == r1.IMAGE_ID and lock["image"] == r1.IMAGE, "Frozen Rover image changed"
    )
    require(
        (lock["fixed_E_m"], lock["fixed_side_m"], lock["fixed_swath_m"]) == (4.5, 90, 67.5),
        "Frozen E/side/swath changed",
    )
    require(lock["historical_lock_sha256"] == r1.sha(HISTORICAL_LOCK), "Historical lock changed")
    containers = set()
    for scenario in ("M00", "M04"):
        fixture = lock["fixtures"][scenario]
        directory = Path(fixture["preparation_directory"]).resolve()
        require(directory.is_relative_to(CLOSURE), "Preparation must use independent FC3 evidence")
        require(
            directory
            == directory.parent.parent
            / run_stage(directory.parent.parent, "preparation")
            / scenario,
            "Preparation directory does not match its FC3 namespace",
        )
        evidence = validate_fixture(directory, scenario, source_head, binary_sha)
        for key, value in evidence.items():
            require(fixture[key] == value, f"Lock fixture {scenario}/{key} mismatch")
        require(fixture["sha256"] == FIXTURE_SHA[scenario], "Lock fixture SHA changed")
        row = load(directory / "run.json")
        require(
            row["status"] == "PASS" and row["active_case_executed"] and row["binary_unchanged"],
            "Preparation did not pass its real test oracle",
        )
        require(
            row["source_head"] == source_head and row["binary_sha256"] == binary_sha,
            "Preparation identity mismatch",
        )
        require(
            row["image_id"] == r1.IMAGE_ID and row["container_stopped"],
            "Preparation container not frozen/stopped",
        )
        docker_args = row["docker_command"]
        expected_name = (
            f"qgc-v05-10-r1-{run_stage(directory.parent.parent, 'preparation')}-{scenario.lower()}"
        )
        require(
            docker_args[docker_args.index("--name") + 1] == expected_name,
            "Preparation container does not match its FC3 namespace",
        )
        require(row["container_id"] not in containers, "Preparation reused a container")
        containers.add(row["container_id"])


def create_lock(output: Path, source_head: str, binary_sha: str) -> dict:
    preparation = load(output / "fc3-preparation.json")
    require(preparation["source_head"] == source_head, "Preparation source mismatch")
    require(preparation["binary_sha256"] == binary_sha, "Preparation binary mismatch")
    require(preparation["prepare_only"] is True, "A fresh preparation run is required")
    require(
        preparation["preparation_stage"] == run_stage(output, "preparation"),
        "Preparation metadata namespace mismatch",
    )
    fixtures = {}
    for scenario in ("M00", "M04"):
        directory = output / run_stage(output, "preparation") / scenario
        fixtures[scenario] = validate_fixture(directory, scenario, source_head, binary_sha)
        fixtures[scenario].update(
            sha256=FIXTURE_SHA[scenario], preparation_directory=str(directory)
        )
    lock = {
        "locked_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source_head": source_head,
        "binary_sha256": binary_sha,
        "protocol_sha256": r1.sha(r1.PROTOCOL),
        "historical_lock_sha256": r1.sha(HISTORICAL_LOCK),
        "image": r1.IMAGE,
        "image_id": r1.IMAGE_ID,
        "fixed_E_m": 4.5,
        "fixed_side_m": 90,
        "fixed_swath_m": 67.5,
        "fixtures": fixtures,
    }
    validate_lock(lock, source_head, binary_sha)
    return lock


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "lock", "validate-lock", "accept"))
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--fixture-lock", type=Path, required=True)
    parser.add_argument("--source-head", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--scenario", choices=SCENARIOS)
    args = parser.parse_args()
    output, lock_path, binary = (
        path.resolve() for path in (args.output_root, args.fixture_lock, args.binary)
    )
    validate_paths(output, lock_path)
    validate_frozen_inputs()
    check_identity(args.source_head, binary, args.binary_sha256)
    require(
        args.action == "accept" or args.scenario is None, "--scenario is only valid for acceptance"
    )
    r1.OUT, r1.EXE, r1.FIXTURE_LOCK = output, binary, lock_path
    preparation_stage = run_stage(output, "preparation")
    acceptance_stage = run_stage(output, "acceptance")
    if args.action == "prepare":
        require(args.scenario is None, "Preparation requires both M00 and M04")
        require(not lock_path.exists(), "Do not prepare over an existing fixture lock")
        require(
            not (output / preparation_stage).exists(), "Do not overwrite an existing preparation"
        )
        require(
            not (output / "fc3-preparation.json").exists(), "Do not overwrite preparation metadata"
        )
        rows = []
        for scenario in ("M00", "M04"):
            check_identity(args.source_head, binary, args.binary_sha256)
            row = r1.run_scenario(preparation_stage, scenario, prepare_only=True)
            check_identity(args.source_head, binary, args.binary_sha256)
            require(
                row["status"] == "PASS" and row["container_stopped"], "Preparation failed; stop"
            )
            validate_fixture(
                output / preparation_stage / scenario,
                scenario,
                args.source_head,
                args.binary_sha256,
            )
            rows.append(row)
        write_new(
            output / "fc3-preparation.json",
            {
                "source_head": args.source_head,
                "binary_sha256": args.binary_sha256,
                "prepare_only": True,
                "preparation_stage": preparation_stage,
                "runs": rows,
            },
        )
    elif args.action == "lock":
        lock = create_lock(output, args.source_head, args.binary_sha256)
        check_identity(args.source_head, binary, args.binary_sha256)
        write_new(lock_path, lock)
    else:
        lock = load(lock_path)
        validate_lock(lock, args.source_head, args.binary_sha256)
        if args.action == "accept":
            for scenario in [args.scenario] if args.scenario else SCENARIOS:
                check_identity(args.source_head, binary, args.binary_sha256)
                row = r1.run_scenario(acceptance_stage, scenario)
                check_identity(args.source_head, binary, args.binary_sha256)
                require(
                    row["status"] == "PASS" and row["container_stopped"],
                    "Acceptance failed; stop without tuning fixture",
                )
                if scenario in ("M00", "M04"):
                    evidence = validate_fixture(
                        output / acceptance_stage / scenario,
                        scenario,
                        args.source_head,
                        args.binary_sha256,
                    )
                    for key in (
                        "canonical",
                        "canonicalSha256",
                        "taskSha256",
                        "artifactSha256",
                        "taskIdentity",
                    ):
                        require(
                            evidence[key] == lock["fixtures"][scenario][key],
                            f"Acceptance/lock {key} mismatch",
                        )
    print(json.dumps({"action": args.action, "status": "PASS"}), flush=True)


if __name__ == "__main__":
    main()
