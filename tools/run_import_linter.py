"""Run import-linter from the tools directory on every supported host platform."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path


def main() -> int:
    tools_dir = Path(__file__).resolve().parent
    env = os.environ.copy()
    env["PYTHONPATH"] = "."

    return subprocess.run(
        ["lint-imports", "--config", "pyproject.toml"],
        cwd=tools_dir,
        env=env,
        check=False,
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
