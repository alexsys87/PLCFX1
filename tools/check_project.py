#!/usr/bin/env python3
"""Check register/CMSIS IAR inputs and the BluePill memory contract.

This is a static consistency check, not an IAR build, link, or flash-loader test.
"""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
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


def macro_value(path, name):
    text = path.read_text(encoding="utf-8")
    match = re.search(r"^\s*#define\s+" + re.escape(name) +
                      r"\s+(0x[0-9a-fA-F]+|[0-9]+)[uUlL]*\s*$", text, re.M)
    assert match, f"Missing numeric memory definition {name}: {path}"
    return int(match[1], 0)


def check_memory():
    path = EWARM / "bluepill_plc.icf"
    text = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8"), flags=re.S)
    symbols = dict(re.findall(r"define\s+symbol\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+|[0-9]+)\s*;", text))
    expected = {
        "__ICFEDIT_intvec_start__": 0x08000000,
        "__ICFEDIT_region_ROM_start__": 0x08000000,
        "__ICFEDIT_region_ROM_end__": 0x0800BFFF,
        "__ICFEDIT_region_RAM_start__": 0x20000000,
        "__ICFEDIT_region_RAM_end__": 0x20001BFF,
        "__ICFEDIT_size_cstack__": 0x800,
        "__ICFEDIT_size_heap__": 0,
    }
    for name, value in expected.items():
        assert name in symbols and int(symbols[name], 0) == value, f"Wrong ICF symbol: {name}"
    # Deliberately validate the reviewed placement contract, not arbitrary ICF syntax.
    statements = {re.sub(r"\s+", "", s) for s in text.split(";") if s.strip()}
    required = [
        "define memory mem with size = 4G",
        "define region ROM_region = mem:[from __ICFEDIT_region_ROM_start__ to __ICFEDIT_region_ROM_end__]",
        "define region RAM_region = mem:[from __ICFEDIT_region_RAM_start__ to __ICFEDIT_region_RAM_end__]",
        "define region PLC_ROM_region = mem:[from 0x0800C000 to 0x0800F7FF]",
        "define region PLC_STEPS_region = mem:[from 0x20001C00 to 0x20001FFF]",
        "define region PLC_RAM_region = mem:[from 0x20002000 to 0x20004FFF]",
        "define block CSTACK with alignment = 8, size = __ICFEDIT_size_cstack__ { }",
        "define block HEAP with alignment = 8, size = __ICFEDIT_size_heap__ { }",
        "initialize by copy { readwrite }",
        "do not initialize { section .plc_ram, section .plc_steps }",
        "keep { section .plc_rom, section .plc_steps, section .plc_ram }",
        "place at address mem:__ICFEDIT_intvec_start__ { readonly section .intvec }",
        "place at start of PLC_ROM_region { readonly section .plc_rom }",
        "place at start of PLC_STEPS_region { section .plc_steps }",
        "place at start of PLC_RAM_region { section .plc_ram }",
        "place in ROM_region { readonly }",
        "place in RAM_region { readwrite, block CSTACK, block HEAP }",
    ]
    contract = {re.sub(r"\s+", "", s) for s in required}
    contract.update(re.sub(r"\s+", "", f"define symbol {n} = {symbols[n]}") for n in expected)
    assert statements == contract, "ICF differs from the bounded, non-overlapping memory contract"
    for name, value in {
        "PLC_RAM_BASE": 0x20002000, "PLC_RAM_SIZE": 0x3000,
        "PLC_STEPS_BASE": 0x20001C00, "PLC_STEPS_SIZE": 0x400,
    }.items():
        assert macro_value(ROOT / "inc/plc_memory.h", name) == value, name
    for name, value in {
        "PLC_PROGRAM_BASE": 0x0800C000, "PLC_RETAIN_BASE": 0x0800F800,
        "PLC_FLASH_END": 0x08010000, "PLC_FLASH_PAGE_SIZE": 0x400,
        "PLC_FLASH_BLOCK_SIZE": 0x800,
    }.items():
        assert macro_value(ROOT / "inc/plc_flash.h", name) == value, name
    memory = (ROOT / "src/plc_memory.c").read_text(encoding="utf-8")
    for section, variable, size in ((".plc_ram", "plc_ram", "PLC_RAM_SIZE"),
                                    (".plc_steps", "plc_steps", "PLC_STEPS_SIZE")):
        pattern = (r'#pragma\s+location\s*=\s*"' + re.escape(section) +
                   r'"\s+__no_init\s+uint8_t\s+' + variable + r'\[' + size + r'\]')
        assert re.search(pattern, memory), f"Missing IAR no-init reservation: {section}"
    rom = (ROOT / "src/PLC_Com.c").read_text(encoding="utf-8")
    assert re.search(r'#pragma\s+location\s*=\s*"\.plc_rom"\s+__root\s+const\s+u8\s+PLC_ROM\[110\]', rom), "Missing PLC factory header"


def check(preprocess):
    # IAR runs on Windows: tracked file paths and their parent directories
    # must not differ only by case. Ignore empty, untracked directories.
    case_paths = {}
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        relative_path = path.relative_to(ROOT)
        for candidate in (relative_path, *relative_path.parents):
            if candidate == Path("."):
                continue
            relative = str(candidate)
            key = relative.casefold()
            assert key not in case_paths or case_paths[key] == relative, f"Case-colliding paths: {case_paths.get(key)} and {relative}"
            case_paths[key] = relative
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
    for ext, tree in trees.items():
        if ext == ".eww":
            continue
        names = [c.findtext("name") for c in tree.findall("configuration")]
        assert sorted(names) == ["Debug", "Release"], f"Wrong configurations: {ext}"
    startup = ROOT / "CMSIS/device/startup_stm32f103xb.s"
    assert [p for p in sources if p.suffix.lower() == ".s"] == [startup], "Wrong IAR startup"
    expected_sources = {p.resolve() for folder in ("App", "src", "CMSIS/device")
                        for p in (ROOT / folder).rglob("*") if p.suffix.lower() == ".c"}
    assert set(sources) == expected_sources | {startup}, "Build does not include exactly the firmware sources"
    assert not trees[".ewp"].findall(".//excluded"), "Unexpected configuration-specific exclusion"
    for config in configs:
        includes = [project_path(s) for s in option_states(config, "CCIncludePath2")]
        for path in includes:
            assert path.is_dir(), f"Missing include directory: {path}"
        defines = option_states(config, "CCDefines")
        expected_defines = ["STM32F103xB"] + (["NDEBUG"] if config.findtext("name") == "Release" else [])
        assert defines == expected_defines, f"Wrong device/SPL defines: {defines}"
        assert includes == [ROOT / "inc", ROOT / "CMSIS", ROOT / "CMSIS/device"], "Wrong CMSIS include paths/order"
        assert option_states(config, "OGChipSelectEditMenu") == ["STM32F103C8\tST STM32F103C8"], "Wrong target device"
        assert option_states(config, "IlinkIcfOverride") == ["1"], "Custom ICF disabled"
        assert option_states(config, "IlinkIcfFile") == ["$PROJ_DIR$\\bluepill_plc.icf"], "Wrong ICF"
        assert option_states(config, "CCUseIarExtensions") == ["1"], "IAR no-init extensions disabled"
    for config in trees[".ewd"].findall("configuration"):
        for name, expected_value in {
            "MemFile": "$TOOLKIT_DIR$\\config\\debugger\\ST\\STM32F103C8.ddf",
            "OCDynDriverList": "STLINK_ID",
            "FlashLoadersV3": "$TOOLKIT_DIR$/config/flashloader/ST/FlashSTM32F10xx8.board",
            "MassEraseBeforeFlashing": "0", "CCSTLinkInterfaceRadio": "1",
        }.items():
            assert option_states(config, name) == [expected_value], f"Wrong {config.findtext('name')} debugger option: {name}"
    check_memory()
    for folder in ("App", "inc", "src", "CMSIS"):
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and path.suffix.lower() in (".c", ".h", ".s", ".inc"):
                text = path.read_text(encoding="utf-8")
                if folder in ("App", "inc", "src"):
                    code = COMMENT_OR_LITERAL.sub(lambda m: m[0] if m[0].startswith('"') else " ", text)
                    assert not re.search(r"USE_STDPERIPH_DRIVER|STM32F10X_(?:HD|MD|LD)|stm32f10x(?:_[a-z0-9]+)?\.h", code), f"Legacy SPL dependency: {path}"
                    # NVIC_* is CMSIS; RTC_Init/Get/Set are project-owned APIs.
                    # Object-like register macros followed by '(' are not calls.
                    calls = re.sub(r"^\s*#.*$", "", code, flags=re.M)
                    assert not re.search(r"\b(?:(?:RCC|GPIO|USART|TIM|FLASH|EXTI|BKP|PWR|DMA|ADC)_[A-Za-z0-9_]+|NVIC_(?:Init|PriorityGroupConfig))\s*\(", calls), f"Possible SPL call: {path}"
                for token in COMMENT_OR_LITERAL.finditer(text):
                    # Preserve upstream CMSIS copyright/typographic characters.
                    if folder in ("App", "inc", "src") and token[0].startswith(("//", "/*")):
                        assert token[0].isascii(), f"Non-English or damaged comment: {path}"
                        assert "???" not in token[0], f"Unreadable comment: {path}"
    print(f"OK: {len(sources)} source paths, CMSIS-only Debug/Release, matching ST-Link/SWD sidecars and English comments")
    print("OK: bounded 48K firmware Flash / 7K static RAM+stack, PLC sections, 64K Flash / 20K RAM limits")
    print("NOTE: .plc_rom is a factory download image at C000; retained F800..FFFF is not linked")
    if preprocess:
        compiler = shutil.which("gcc")
        assert compiler, "Install GCC to use --preprocess"
        # Cloud-backed project files otherwise get fetched for every GCC invocation.
        # Cache the reviewed inputs locally once; never modify the original sources.
        with tempfile.TemporaryDirectory(prefix="bluepill-check-") as directory:
            cached = Path(directory)
            for folder in ("App", "src", "inc", "CMSIS"):
                for path in (ROOT / folder).rglob("*"):
                    if path.is_file():
                        destination = cached / path.relative_to(ROOT)
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(path, destination)
            for config in configs:
                defines = option_states(config, "CCDefines")
                includes = [cached / project_path(s).relative_to(ROOT)
                            for s in option_states(config, "CCIncludePath2")]
                for source in sources:
                    if source.suffix.lower() != ".c":
                        continue
                    command = [compiler, "-E", "-x", "c"]
                    command += ["-D" + d for d in defines]
                    command += ["-I" + str(d) for d in includes]
                    command += [str(cached / source.relative_to(ROOT)), "-o", "/dev/null"]
                    subprocess.run(command, check=True, capture_output=True, text=True, timeout=30)
        print("OK: all C files preprocessed in both configurations (not an IAR build)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preprocess", action="store_true", help="Also check includes using GCC")
    args = parser.parse_args()
    try:
        check(args.preprocess)
    except (AssertionError, OSError, ET.ParseError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"FAILED: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        sys.exit(1)
