#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shlex
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List


@dataclass
class IntegrationCase:
  name: str
  mode: str
  input: str
  expected: str
  compile_flags: List[str]
  tool_args: List[str]
  clang_tidy_args: List[str]
  expect_stdout_contains: List[str]
  expect_stderr_contains: List[str]
  forbid_stdout_contains: List[str]
  forbid_stderr_contains: List[str]
  expected_return_code: int


class CaseFailure(Exception):
  pass


def load_cases(cases_path: Path) -> list[IntegrationCase]:
  raw_cases = json.loads(cases_path.read_text())
  cases: list[IntegrationCase] = []
  for entry in raw_cases:
    cases.append(
        IntegrationCase(
            name=entry["name"],
            mode=entry["mode"],
            input=entry["input"],
            expected=entry["expected"],
            compile_flags=entry.get("compile_flags", []),
            tool_args=entry.get("tool_args", []),
            clang_tidy_args=entry.get("clang_tidy_args", []),
            expect_stdout_contains=entry.get("expect_stdout_contains", []),
            expect_stderr_contains=entry.get("expect_stderr_contains", []),
            forbid_stdout_contains=entry.get("forbid_stdout_contains", []),
            forbid_stderr_contains=entry.get("forbid_stderr_contains", []),
            expected_return_code=entry.get("expected_return_code", 0),
        )
    )
  return cases


def locate_plugin(build_dir: Path) -> Path:
  candidates = [
      build_dir / "libeast-const-tidy.so",
      build_dir / "libeast-const-tidy.dylib",
      build_dir / "libeast-const-tidy.dll",
  ]
  for candidate in candidates:
    if candidate.exists():
      return candidate
  raise CaseFailure(
      "Unable to locate the east-const-tidy module. Expected one of: "
      + ", ".join(str(c) for c in candidates)
  )


def ensure_artifact(path: Path, description: str) -> None:
  if not path.exists():
    raise CaseFailure(f"Missing {description} at {path}")


def write_compile_commands(directory: Path, source: Path, flags: list[str]) -> None:
  compile_commands = [
      {
          "directory": str(directory),
          "command": "clang++ " + " ".join(flags + [source.name]),
          "file": str(source),
      }
  ]
  (directory / "compile_commands.json").write_text(json.dumps(compile_commands, indent=2))


def check_substrings(stream_name: str, haystack: str, required: Iterable[str], forbidden: Iterable[str], case_name: str) -> None:
  for needle in required:
    if needle not in haystack:
      raise CaseFailure(
          f"Case '{case_name}' expected '{needle}' in {stream_name} but it was missing.\n{stream_name}:\n{haystack}"
      )
  for needle in forbidden:
    if needle in haystack:
      raise CaseFailure(
          f"Case '{case_name}' forbids '{needle}' in {stream_name} but it was present.\n{stream_name}:\n{haystack}"
      )


def compare_files(expected: Path, actual: Path, case_name: str) -> None:
  expected_text = expected.read_text()
  actual_text = actual.read_text()
  if expected_text == actual_text:
    return
  import difflib

  diff = "".join(
      difflib.unified_diff(
          expected_text.splitlines(keepends=True),
          actual_text.splitlines(keepends=True),
          fromfile=str(expected),
          tofile=str(actual),
      )
  )
  raise CaseFailure(
      f"Case '{case_name}' produced unexpected edits. Diff follows:\n{diff}"
  )


def build_command(case: IntegrationCase, *, tool_path: Path, plugin_path: Path, clang_tidy: str, source: Path) -> List[str]:
  base_flags = case.compile_flags
  if case.mode == "standalone-fix":
    return [str(tool_path), *case.tool_args, "-fix", str(source), "--", *base_flags]
  if case.mode == "clang-tidy-fix":
    parts = [
        clang_tidy,
        "-load",
        str(plugin_path),
        "-checks=-*,east-const-enforcer",
        *case.clang_tidy_args,
    ]
    if "-fix" not in case.clang_tidy_args:
      parts.append("-fix")
    parts.extend([str(source), "--", *base_flags])
    return parts
  if case.mode == "clang-tidy-lint":
    return [
        clang_tidy,
        "-load",
        str(plugin_path),
        "-checks=-*,east-const-enforcer",
        *case.clang_tidy_args,
        str(source),
        "--",
        *base_flags,
    ]
  raise CaseFailure(f"Unsupported mode '{case.mode}' in case '{case.name}'")


def format_command(cmd: List[str]) -> str:
  return " ".join(shlex.quote(part) for part in cmd)


def maybe_write_compile_commands(case: IntegrationCase, tmp_dir: Path, source: Path) -> None:
  if case.mode.startswith("clang-tidy"):
    write_compile_commands(tmp_dir, source, case.compile_flags)


def run_case(
    case: IntegrationCase,
    *,
    fixtures_dir: Path,
    tool_path: Path,
    plugin_path: Path,
    clang_tidy: str,
  verbose: bool,
) -> None:
  input_path = fixtures_dir / case.input
  expected_path = fixtures_dir / case.expected
  ensure_artifact(input_path, f"fixture '{case.input}'")
  ensure_artifact(expected_path, f"fixture '{case.expected}'")

  with tempfile.TemporaryDirectory(prefix=f"east-const-{case.name}-") as tmp:
    tmp_dir = Path(tmp)
    work_file = tmp_dir / "input.cpp"
    shutil.copyfile(input_path, work_file)

    maybe_write_compile_commands(case, tmp_dir, work_file)
    cmd = build_command(case, tool_path=tool_path, plugin_path=plugin_path, clang_tidy=clang_tidy, source=work_file)
    if verbose:
      print(f"[CMD] {format_command(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True, cwd=tmp_dir)

    if result.returncode != case.expected_return_code:
      raise CaseFailure(
          f"Case '{case.name}' exited with {result.returncode}, expected {case.expected_return_code}.\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
      )

    check_substrings("STDOUT", result.stdout, case.expect_stdout_contains, case.forbid_stdout_contains, case.name)
    check_substrings("STDERR", result.stderr, case.expect_stderr_contains, case.forbid_stderr_contains, case.name)

    compare_files(expected_path, work_file, case.name)



def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description="Run east-const integration tests.")
  parser.add_argument("--build-dir", default=None, help="Path to the CMake build directory.")
  parser.add_argument(
      "--cases",
      default=None,
      help="Optional path to a JSON file describing integration cases (defaults to tests/integration/cases.json)",
  )
  parser.add_argument(
      "--clang-tidy",
      default="clang-tidy",
      help="Path to the clang-tidy executable (defaults to first clang-tidy on PATH).",
  )
  parser.add_argument(
      "--case",
      action="append",
      dest="selected_cases",
      default=[],
      help="Run only the named case (can be passed multiple times).",
  )
  parser.add_argument(
      "--list-cases",
      action="store_true",
      help="List available cases and exit.",
  )
  parser.add_argument(
      "--verbose",
      action="store_true",
      help="Print the exact command that each case executes.",
  )
  args = parser.parse_args()
  if not args.list_cases and not args.build_dir:
    parser.error("--build-dir is required unless --list-cases is specified")
  return args


def main() -> int:
  args = parse_args()
  cases_path = Path(args.cases) if args.cases else Path(__file__).with_name("cases.json")
  fixtures_dir = cases_path.parent / "fixtures"
  cases = load_cases(cases_path)

  if args.list_cases:
    for case in cases:
      print(case.name)
    return 0

  build_dir = Path(args.build_dir).resolve()
  tool_path = build_dir / "east-const-enforcer"
  ensure_artifact(tool_path, "standalone tool")
  plugin_path = locate_plugin(build_dir)

  if args.selected_cases:
    requested = set(args.selected_cases)
    cases_by_name = {case.name: case for case in cases}
    missing = sorted(requested.difference(cases_by_name))
    if missing:
      print("Unknown integration case(s): " + ", ".join(missing), file=sys.stderr)
      return 1
    cases = [cases_by_name[name] for name in args.selected_cases]

  failures: list[str] = []
  for case in cases:
    try:
      run_case(
          case,
          fixtures_dir=fixtures_dir,
          tool_path=tool_path,
          plugin_path=plugin_path,
          clang_tidy=args.clang_tidy,
          verbose=args.verbose,
        )
      print(f"[PASS] {case.name}")
    except CaseFailure as err:
      failures.append(str(err))
      print(f"[FAIL] {case.name}: {err}")

  if failures:
    print("\n".join(failures), file=sys.stderr)
    return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
