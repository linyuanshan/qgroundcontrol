"""Tests for the clang-tidy analyzer."""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING
from unittest.mock import patch

if TYPE_CHECKING:
    from pathlib import Path

from analyzers.clang_tidy import ClangTidyAnalyzer


def completed() -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess([], 0, stdout="", stderr="")


def test_windows_disables_msvc_precompiled_headers(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    source_file = tmp_path / "source.cc"
    analyzer = ClangTidyAnalyzer(tmp_path, build_dir)

    with (
        patch("analyzers.clang_tidy.sys.platform", "win32"),
        patch("analyzers.clang_tidy.run_captured", return_value=completed()) as run,
    ):
        analyzer._analyze_file(source_file)

    run.assert_called_once_with(
        [
            "clang-tidy",
            "-p",
            str(build_dir),
            "--extra-arg=/Y-",
            "--extra-arg=-Wno-unused-command-line-argument",
            str(source_file),
        ]
    )


def test_non_windows_preserves_compilation_command(tmp_path: Path) -> None:
    build_dir = tmp_path / "build"
    source_file = tmp_path / "source.cc"
    analyzer = ClangTidyAnalyzer(tmp_path, build_dir)

    with (
        patch("analyzers.clang_tidy.sys.platform", "linux"),
        patch("analyzers.clang_tidy.run_captured", return_value=completed()) as run,
    ):
        analyzer._analyze_file(source_file)

    run.assert_called_once_with(["clang-tidy", "-p", str(build_dir), str(source_file)])
