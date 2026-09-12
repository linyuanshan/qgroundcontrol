"""Build and run the pinned ArduRover SITL used by Marine P1 validation."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ARDUPILOT_TAG = "Rover-4.7.0"
IMAGE_NAME = "qgc-ardurover-sitl:rover-4.7.0"
CONTAINER_NAME = "qgc-ardurover-sitl"


def build_command(simulation_dir: Path) -> list[str]:
    context = simulation_dir / "ardurover-sitl"
    return [
        "docker",
        "build",
        "--tag",
        IMAGE_NAME,
        "--build-arg",
        f"ARDUPILOT_TAG={ARDUPILOT_TAG}",
        str(context),
    ]


def run_command() -> list[str]:
    return [
        "docker",
        "run",
        "--detach",
        "--name",
        CONTAINER_NAME,
        "--publish",
        "5760:5760",
        IMAGE_NAME,
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


def stop_command() -> list[str]:
    return ["docker", "rm", "--force", CONTAINER_NAME]


def _run(command: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=check, text=True)


def _docker_available() -> bool:
    return shutil.which("docker") is not None


def _image_exists() -> bool:
    result = _run(["docker", "image", "inspect", IMAGE_NAME], check=False)
    return result.returncode == 0


def _container_exists() -> bool:
    result = _run(["docker", "container", "inspect", CONTAINER_NAME], check=False)
    return result.returncode == 0


def _start(simulation_dir: Path, rebuild: bool) -> None:
    if rebuild or not _image_exists():
        _run(build_command(simulation_dir))
    if _container_exists():
        _run(stop_command())
    _run(run_command())
    print("ArduRover SITL is starting. Connect QGroundControl to TCP 127.0.0.1:5760.")
    print(f"Inspect startup with: docker logs --follow {CONTAINER_NAME}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action", choices=("start", "stop", "logs", "status"), nargs="?", default="start"
    )
    parser.add_argument("--rebuild", action="store_true", help="Rebuild the pinned SITL image")
    args = parser.parse_args(argv)

    if not _docker_available():
        print("Docker is required to run ArduRover SITL.", file=sys.stderr)
        return 2

    simulation_dir = Path(__file__).resolve().parent
    if args.action == "start":
        _start(simulation_dir, args.rebuild)
    elif args.action == "stop":
        if _container_exists():
            _run(stop_command())
    elif args.action == "logs":
        _run(["docker", "logs", "--follow", CONTAINER_NAME])
    else:
        _run(["docker", "container", "inspect", "--format", "{{.State.Status}}", CONTAINER_NAME])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
