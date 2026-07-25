#!/usr/bin/env python3
"""
AGI protocol peer for offline pbx3cagi tests.

Writes the AGI env block, spawns pbx3cagi, reads AGI commands from child stdout,
writes mock 200 responses on child stdin. One child process per invocation.

Optional scenario files:
  variables.json  — GET VARIABLE name → value (e.g. DIALSTATUS)
  env.json        — extra process env (e.g. PBX3_FLEET_MODE=1)
"""

from __future__ import annotations

import json
import os
import re
import select
import subprocess
import sys
from pathlib import Path

DB_GET_RE = re.compile(r'^DATABASE GET "([^"]*)" "([^"]*)"\s*$')
GET_VAR_RE = re.compile(r'^GET VARIABLE (\S+)\s*$')


def load_json_map(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    with path.open(encoding="utf-8") as fh:
        data = json.load(fh)
    return {str(k): str(v) for k, v in data.items()}


def load_astdb(scenario_dir: Path) -> dict[str, str]:
    return load_json_map(scenario_dir / "astdb.json")


def load_variables(scenario_dir: Path) -> dict[str, str]:
    """Channel/Asterisk vars answered for GET VARIABLE."""
    defaults = {
        "DEBUG": "",
        "BLINDTRANSFER": "",
        "MOH": "",
        "DIALSTATUS": "",
    }
    defaults.update(load_json_map(scenario_dir / "variables.json"))
    return defaults


def load_env_overlay(scenario_dir: Path) -> dict[str, str]:
    return load_json_map(scenario_dir / "env.json")


def agi_value_response(value: str) -> str:
    if value == "":
        return "200 result=0\n"
    return f"200 result=1 ({value})\n"


def handle_command(line: str, astdb: dict[str, str], variables: dict[str, str]) -> str:
    line = line.rstrip("\r\n")

    m = DB_GET_RE.match(line)
    if m:
        family, key = m.group(1), m.group(2)
        lookup = f"{family}/{key}"
        return agi_value_response(astdb.get(lookup, ""))

    m = GET_VAR_RE.match(line)
    if m:
        var = m.group(1)
        return agi_value_response(variables.get(var, ""))

    # EXEC, SET CONTEXT / EXTENSION / PRIORITY / VARIABLE, WAIT FOR DIGIT, etc.
    return "200 result=0\n"


def run_scenario(
    scenario_dir: Path,
    pbx3cagi: Path,
    transcript_path: Path | None,
) -> tuple[list[str], int]:
    agi_env = (scenario_dir / "agi_env.txt").read_text(encoding="utf-8")
    argv = (scenario_dir / "argv.txt").read_text(encoding="utf-8").split()
    astdb = load_astdb(scenario_dir)
    variables = load_variables(scenario_dir)

    env = os.environ.copy()
    tenant_db = os.environ.get("PBX3CAGI_SQLITE_DB")
    if not tenant_db:
        default_db = scenario_dir.parent.parent / "fixtures" / "tenant" / "sqlite.rdonly.db"
        if default_db.is_file():
            env["PBX3CAGI_SQLITE_DB"] = str(default_db.resolve())
    env.update(load_env_overlay(scenario_dir))

    child = subprocess.Popen(
        [str(pbx3cagi), *argv],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        text=True,
        bufsize=0,
    )

    assert child.stdin is not None
    assert child.stdout is not None

    child.stdin.write(agi_env)
    if not agi_env.endswith("\n\n"):
        if agi_env.endswith("\n"):
            child.stdin.write("\n")
        else:
            child.stdin.write("\n\n")
    child.stdin.flush()

    transcript: list[str] = []
    stdout_fd = child.stdout.fileno()

    while True:
        ready, _, _ = select.select([stdout_fd], [], [], 2.0)
        if not ready:
            if child.poll() is not None:
                break
            continue

        line = child.stdout.readline()
        if line == "":
            if child.poll() is not None:
                break
            continue

        transcript.append(line.rstrip("\r\n"))
        response = handle_command(line, astdb, variables)
        child.stdin.write(response)
        child.stdin.flush()

        if child.poll() is not None:
            while True:
                ready, _, _ = select.select([stdout_fd], [], [], 0.05)
                if not ready:
                    break
                extra = child.stdout.readline()
                if extra == "":
                    break
                transcript.append(extra.rstrip("\r\n"))
            break

    stderr = child.stderr.read() if child.stderr else ""
    code = child.wait()

    if transcript_path is not None:
        transcript_path.write_text("\n".join(transcript) + "\n", encoding="utf-8")
        if stderr.strip():
            transcript_path.write_text(
                transcript_path.read_text(encoding="utf-8")
                + "\n# stderr\n"
                + stderr.strip()
                + "\n",
                encoding="utf-8",
            )

    return transcript, code


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: agi_respond.py <scenario_dir> <path/to/pbx3cagi> [transcript.txt]", file=sys.stderr)
        return 2

    scenario_dir = Path(sys.argv[1]).resolve()
    pbx3cagi = Path(sys.argv[2]).resolve()
    transcript_path = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None

    if not scenario_dir.is_dir():
        print(f"scenario dir not found: {scenario_dir}", file=sys.stderr)
        return 2
    if not pbx3cagi.is_file():
        print(f"pbx3cagi binary not found: {pbx3cagi}", file=sys.stderr)
        return 2

    _, code = run_scenario(scenario_dir, pbx3cagi, transcript_path)
    return code


if __name__ == "__main__":
    sys.exit(main())
