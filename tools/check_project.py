#!/usr/bin/env python3
"""Validate IAR project paths and optionally preprocess every C translation unit."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
EWARM = ROOT / "ewarm"
NAME = "plc_fx1_stm32f103"
COMMENT_OR_LITERAL = re.compile(
    r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*', re.S
)


def project_path(value):
    """Resolve an IAR project-relative path using the project directory."""
    assert value.startswith("$PROJ_DIR$\\"), value
    return (EWARM / value.removeprefix("$PROJ_DIR$\\").replace("\\", "/")).resolve()


def option_states(config, name):
    options = [o for o in config.findall(".//option") if o.findtext("name") == name]
    assert len(options) == 1, name
    return [s.text for s in options[0].findall("state") if s.text]


def check(preprocess):
    trees = {ext: ET.parse(EWARM / (NAME + ext)) for ext in (".ewp", ".ewd", ".ewt", ".eww")}
    assert trees[".eww"].findtext(".//path") == "$WS_DIR$\\" + NAME + ".ewp"
    sources = []
    for ext in (".ewp", ".ewt"):
        paths = [project_path(f.findtext("name")) for f in trees[ext].findall(".//file")]
        assert paths, "Project must contain source files"
        for path in paths:
            assert path.is_file(), f"Missing {ext} source: {path}"
        assert len(paths) == len(set(paths)), f"Duplicate sources in {ext}"
        if ext == ".ewp":
            sources = paths
        else:
            assert set(paths) == set(sources), "C-STAT and build source lists differ"
    configs = trees[".ewp"].findall("configuration")
    assert {c.findtext("name") for c in configs} == {"Debug", "Release"}
    for config in configs:
        includes = [project_path(s) for s in option_states(config, "CCIncludePath2")]
        for path in includes:
            assert path.is_dir(), f"Missing include directory: {path}"
        defines = option_states(config, "CCDefines")
        assert {"USE_STDPERIPH_DRIVER", "STM32F10X_HD"}.issubset(defines)
        assert option_states(config, "OGChipSelectEditMenu") == option_states(configs[0], "OGChipSelectEditMenu")
        assert option_states(config, "IlinkIcfFile") == option_states(configs[0], "IlinkIcfFile")
        if preprocess:
            compiler = shutil.which("gcc")
            assert compiler, "Install GCC to use --preprocess"
            for source in sources:
                if source.suffix.lower() != ".c":
                    continue
                command = [compiler, "-E", "-x", "c"]
                command += ["-D" + d for d in defines]
                command += ["-I" + str(d) for d in includes]
                command += [str(source), "-o", "/dev/null"]
                subprocess.run(command, check=True, capture_output=True, text=True)
    for folder in ("App", "inc", "src", "ST", "CMSIS", "STM32F10x_StdPeriph_Driver"):
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and path.suffix.lower() in (".c", ".h", ".s"):
                text = path.read_text(encoding="utf-8")
                for token in COMMENT_OR_LITERAL.finditer(text):
                    if token[0].startswith(("//", "/*")):
                        assert token[0].isascii(), f"Non-English or damaged comment: {path}"
                        assert "???" not in token[0], f"Unreadable comment: {path}"
    print(f"OK: {len(sources)} source paths, Debug/Release include paths, sidecars and English comments")
    if preprocess:
        print("OK: all C files preprocessed in both configurations (not an IAR build)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preprocess", action="store_true", help="Also check includes using GCC")
    args = parser.parse_args()
    try:
        check(args.preprocess)
    except (AssertionError, OSError, ET.ParseError, subprocess.CalledProcessError) as error:
        print(f"FAILED: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        sys.exit(1)
