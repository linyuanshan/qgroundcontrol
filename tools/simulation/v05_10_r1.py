"""V05-10-R1: 固定测量集和独立 fresh SITL 容器的证据 runner。"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import math
import os
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/v05-10-r1"
FIXTURE_LOCK = OUT / "fixture-lock-final.json"
EXE = ROOT / "build/P0-01-marine-debug/Debug/QGroundControl.exe"
IMAGE = "qgc-ardurover-sitl:rover-4.7.0"
IMAGE_ID = "sha256:001f20d07215138f2cb4aebc8eb691c8f4c3a23825a01f343b3f5397d262d3f0"
PROTOCOL = ROOT / "docs/marine/V05_10_R1_EXECUTION_CALIBRATION.md"
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def command(args: list[str]) -> str:
    return subprocess.check_output(args, cwd=ROOT, text=True, creationflags=FLAGS).strip()


def run_scenario(stage: str, scenario: str, prepare_only: bool = False) -> dict:
    directory = OUT / stage / scenario
    directory.mkdir(parents=True, exist_ok=False)
    identity = sha(EXE)
    source = command(["git", "rev-parse", "HEAD"])
    assert command(["docker", "image", "inspect", IMAGE, "--format", "{{.Id}}"]) == IMAGE_ID
    name = f"qgc-v05-10-r1-{stage}-{scenario.lower()}"
    docker_args = [
        "docker",
        "run",
        "--detach",
        "--name",
        name,
        "--publish",
        "5760:5760",
        IMAGE,
        "-v",
        "Rover",
        "-f",
        "rover",
        "--no-rebuild",
        "--no-mavproxy",
        "-w",
        "--speedup",
        "1",
        "--custom-location=47.397742,8.545594,488,0",
    ]
    row = {
        "scenario": scenario,
        "source_head": source,
        "binary_sha256": identity,
        "protocol_sha256": sha(PROTOCOL),
        "docker_command": docker_args,
        "image_id": IMAGE_ID,
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    }
    container = command(docker_args)
    row["container_id"] = container
    write(directory / "runner-start.json", row)
    try:
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            probe = subprocess.run(
                ["docker", "exec", container, "cat", "/tmp/Rover.log"],
                capture_output=True,
                text=True,
                creationflags=FLAGS,
            )
            logs = probe.stdout
            if "bind port 5760" in logs or "Waiting for connection" in logs:
                break
            assert (
                command(["docker", "inspect", container, "--format", "{{.State.Running}}"])
                == "true"
            )
            time.sleep(0.5)
        else:
            raise RuntimeError("fresh SITL TCP readiness not observed")
        env = os.environ.copy()
        for key in list(env):
            if key.startswith("QGC_P2_") or key.startswith("QGC_V05_10_"):
                del env[key]
        env.update(
            QT_QPA_PLATFORM="offscreen",
            QT_QUICK_BACKEND="software",
            QSG_RHI_BACKEND="d3d11",
            QT_LOGGING_RULES="*.debug=false",
            QT_QPA_FONTDIR="C:/Windows/Fonts",
            QGC_V05_10_SITL_EVIDENCE_DIR=str(directory.parent),
            QGC_V05_10_SITL_SCENARIO=scenario,
            QGC_V05_10_FRESH_CONTAINER_ID=container,
            QGC_V05_10_SOURCE_HEAD=source,
        )
        if prepare_only:
            env["QGC_V05_10_PREPARE_ONLY"] = "1"
        elif scenario in ("M00", "M04"):
            env["QGC_V05_10_FIXTURE_LOCK"] = str(FIXTURE_LOCK)
        args = [
            str(EXE),
            "--unittest:MarineSITLValidationTest",
            "--allow-multiple",
            f"--unittest-output:{(directory / 'MarineSITLValidationTest.xml').as_posix()}",
        ]
        row["command"] = args
        with (
            (directory / "stdout.log").open("xb") as stdout,
            (directory / "stderr.log").open("xb") as stderr,
        ):
            result = subprocess.run(
                args,
                cwd=ROOT,
                env=env,
                stdout=stdout,
                stderr=stderr,
                timeout=1100,
                creationflags=FLAGS,
            )
        row["exit_code"] = result.returncode
        xml = ET.parse(
            directory / "MarineSITLValidationTest-MarineSITLValidationTest.xml"
        ).getroot()
        row["xml_counts"] = {
            key: int(xml.attrib.get(key, 0)) for key in ("tests", "failures", "errors", "skipped")
        }
        active = [
            case for case in xml.iter("testcase") if case.attrib["name"] == "_validateV05Scenarios"
        ]
        row["active_case_executed"] = len(active) == 1 and not active[0].findall("skipped")
        row["status"] = (
            "PASS"
            if (
                result.returncode == 0
                and not row["xml_counts"]["failures"]
                and not row["xml_counts"]["errors"]
                and row["active_case_executed"]
            )
            else "FAIL"
        )
        row["binary_unchanged"] = identity == sha(EXE)
        assert row["binary_unchanged"]
        print(
            json.dumps({"scenario": scenario, "status": row["status"], "container_id": container}),
            flush=True,
        )
        return row
    except BaseException as error:
        row["status"] = "ERROR"
        row["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        with (directory / "rover.log").open("xb") as log:
            subprocess.run(
                ["docker", "exec", container, "cat", "/tmp/Rover.log"],
                stdout=log,
                stderr=subprocess.STDOUT,
                creationflags=FLAGS,
            )
        with (directory / "container.log").open("xb") as log:
            subprocess.run(
                ["docker", "logs", container],
                stdout=log,
                stderr=subprocess.STDOUT,
                creationflags=FLAGS,
            )
        (directory / "container-inspect.json").write_text(
            command(["docker", "inspect", container]), encoding="utf-8"
        )
        stopped = subprocess.run(
            ["docker", "stop", "--time", "10", container],
            capture_output=True,
            text=True,
            creationflags=FLAGS,
        )
        row["stop_exit_code"] = stopped.returncode
        row["container_stopped"] = (
            command(["docker", "inspect", container, "--format", "{{.State.Running}}"]) == "false"
        )
        row["completed_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        write(directory / "run.json", row)


def calibrate(stage: str) -> None:
    rows = []
    for scenario in ("CAL20", "CAL60", "CAL120"):
        row = run_scenario(stage, scenario)
        rows.append(row)
        assert row["status"] == "PASS", "calibration execution failed; no E selected"
    measured = [
        json.loads((OUT / stage / row["scenario"] / "result.json").read_text()) for row in rows
    ]
    d = max(r["maximumPathDeviationM"] for r in measured)
    v = max(r["maximumSpeedMps"] for r in measured)
    t = max(r["maximumPositionIntervalMs"] for r in measured) / 1000
    assert all(math.isfinite(x) for x in (d, v, t)) and v <= 6 and t <= 0.5
    e = math.ceil((1.25 * d + v * t + 0.02) / 0.5) * 0.5
    side = math.ceil(max(40, 20 * e, 4 * e * e) / 10) * 10
    decision = {
        "protocol_sha256": sha(PROTOCOL),
        "measurement_runs": rows,
        "Dmax_m": d,
        "Vmax_mps": v,
        "Tmax_s": t,
        "selected_E_m": e,
        "selected_side_m": side,
        "selected_swath_m": side * 0.75,
        "owner_design_decision_required": not (0 < e <= 6 and side <= 120),
    }
    write(OUT / "calibration-decision.json", decision)
    print(json.dumps(decision, ensure_ascii=False), flush=True)
    assert not decision["owner_design_decision_required"], "OWNER_DESIGN_DECISION_REQUIRED"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("calibrate", "prepare", "accept"))
    parser.add_argument("--scenario", choices=("M00", "M04", "M05", "M08", "M09", "STALE"))
    parser.add_argument("--stage", help="唯一证据目录名: 重试不得覆盖既有证据")
    args = parser.parse_args()
    stage = args.stage or ("calibration" if args.action == "calibrate" else "acceptance")
    assert stage.replace("-", "").isalnum()
    if args.action == "calibrate":
        calibrate(stage)
    elif args.action == "prepare":
        assert (OUT / "calibration-decision.json").exists()
        for scenario in ("M00", "M04"):
            row = run_scenario(args.stage or "preparation", scenario, prepare_only=True)
            assert row["status"] == "PASS", "fixed positive-E fixture planning failed; stop"
    else:
        lock = json.loads(FIXTURE_LOCK.read_text())
        assert lock["binary_sha256"] == sha(EXE) and lock["protocol_sha256"] == sha(PROTOCOL)
        for scenario in (
            [args.scenario] if args.scenario else ["M00", "M04", "M05", "M08", "M09", "STALE"]
        ):
            row = run_scenario(stage, scenario)
            assert row["status"] == "PASS", "acceptance failed: stop without changing E/fixture"
            if scenario in ("M00", "M04"):
                realized = json.loads((OUT / stage / scenario / "scenario.json").read_text())
                assert realized["canonical"] == lock["fixtures"][scenario]["canonical"]
                assert realized["canonicalSha256"] == lock["fixtures"][scenario]["sha256"]
                assert realized["binarySha256"] == lock["binary_sha256"]
                assert realized["taskSha256"] == lock["fixtures"][scenario]["taskSha256"]
                assert realized["artifactSha256"] == lock["fixtures"][scenario]["artifactSha256"]


if __name__ == "__main__":
    main()
