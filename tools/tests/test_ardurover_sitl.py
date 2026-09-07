from pathlib import Path

from simulation import ardurover_sitl


def test_build_command_pins_ardurover_release() -> None:
    command = ardurover_sitl.build_command(Path("tools/simulation"))

    assert command[:2] == ["docker", "build"]
    assert f"ARDUPILOT_TAG={ardurover_sitl.ARDUPILOT_TAG}" in command
    assert command[-1] == str(Path("tools/simulation/ardurover-sitl"))


def test_run_command_starts_rover_sitl_for_qgc() -> None:
    command = ardurover_sitl.run_command()

    assert command[:3] == ["docker", "run", "--detach"]
    assert command[command.index("--publish") : command.index("--publish") + 2] == [
        "--publish",
        "5760:5760",
    ]
    assert "Rover" in command
    assert "rover" in command
    assert "--no-mavproxy" in command
    assert "--no-rebuild" in command


def test_stop_command_targets_only_ardurover_container() -> None:
    assert ardurover_sitl.stop_command() == [
        "docker",
        "rm",
        "--force",
        ardurover_sitl.CONTAINER_NAME,
    ]


def test_dockerfile_builds_the_pinned_rover_target() -> None:
    dockerfile = Path(__file__).parents[1] / "simulation" / "ardurover-sitl" / "Dockerfile"
    contents = dockerfile.read_text(encoding="utf-8")

    assert f"ARG ARDUPILOT_TAG={ardurover_sitl.ARDUPILOT_TAG}" in contents
    assert "./waf rover" in contents
    assert 'ENTRYPOINT ["/ardupilot/Tools/autotest/sim_vehicle.py"]' in contents
