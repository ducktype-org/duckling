# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import json
import pathlib
import re
import shlex
import subprocess as sp
import tempfile
from dataclasses import asdict, dataclass, field

import click

from ..helpers import exit_with_error

DEFAULT_LLVM_BIN = pathlib.Path("scripts/downloads/llvm_lib_19.1.7_native/bin")
DEFAULT_SOURCES = [
    "src/vm/src/vm/core/fast/eval/instruction_functions.cpp",
    "src/vm/src/vm/core/fast/program/instructions/executable.cpp",
]
DEFAULT_FILTER = r"FastExecutor::(Instr_\w+)|\b(dispatcher_\w+)|\b(stencil_\w+)"
VARIANT_DEFINES = {"tc": "-DUSE_TAIL_CALLS", "sc": "-DUSE_SWITCH_CASE"}
UNWIND_MARKERS = ("__cxa_throw", "__cxa_rethrow", "_Unwind_Resume")


@dataclass
class FunctionStats:
    name: str
    ir_instructions: int = 0
    basic_blocks: int = 0
    calls: list[str] = field(default_factory=list)
    unwind: int = 0
    x86_instructions: int = 0
    bytes: int = 0
    rthroughput: float | None = None


def _tool(llvm_bin: pathlib.Path, name: str) -> str:
    path = llvm_bin / name
    if not path.exists():
        exit_with_error(f"`{path}` does not exist, run `./toolbox.py download-llvm` or pass --llvm-bin")
    return str(path)


def _run(cmd: list[str], input: str | None = None) -> str:
    result = sp.run(cmd, capture_output=True, text=True, input=input)
    if result.returncode != 0:
        exit_with_error(f"`{shlex.join(cmd[:3])} ...` failed:\n{result.stderr}")
    return result.stdout


def _compile_flags(build_dir: pathlib.Path, source: pathlib.Path, variant: str) -> list[str]:
    """Picks the include paths and definitions of `source` from the build's compile_commands.json."""
    db_path = build_dir / "compile_commands.json"
    if not db_path.exists():
        exit_with_error(f"`{db_path}` does not exist, configure the build first")

    define = VARIANT_DEFINES[variant]
    for entry in json.loads(db_path.read_text()):
        if pathlib.Path(entry["file"]).resolve() != source.resolve():
            continue
        tokens = shlex.split(entry["command"]) if "command" in entry else entry["arguments"]
        if define not in tokens:
            continue

        flags: list[str] = []
        it = iter(tokens[1:])
        for token in it:
            if token == "-isystem":
                flags += [token, next(it)]
            elif token == "-o":
                next(it)
            elif token.startswith(("-D", "-I", "-std")) or token == "-fPIC":
                flags.append(token)
        return flags

    exit_with_error(f"No `{variant}` compile command for `{source}` in `{db_path}`")


def _split_functions(text: str, begin: re.Pattern, end: re.Pattern) -> dict[str, list[str]]:
    functions: dict[str, list[str]] = {}
    current: list[str] | None = None
    for line in text.splitlines():
        if current is None:
            if m := begin.match(line):
                current = functions.setdefault(m.group(1), [])
        elif end.match(line):
            current = None
        else:
            current.append(line)
    return functions


def _demangle(llvm_bin: pathlib.Path, names: list[str]) -> dict[str, str]:
    out = _run([_tool(llvm_bin, "llvm-cxxfilt")], input="\n".join(names))
    return dict(zip(names, out.splitlines()))


IR_DEFINE = re.compile(r'^define [^@]*@("?[^"(]+"?)\(')
IR_CALL = re.compile(r'\b(?:call|invoke)\b[^@]*@("?[\w.$]+"?)\(')


def _ir_stats(ir: str, mangled: str) -> FunctionStats:
    stats = FunctionStats(name=mangled)
    body = _split_functions(ir, IR_DEFINE, re.compile(r"^}"))[mangled]
    stats.basic_blocks = 1
    for line in body:
        line = line.strip()
        if not line or line.startswith(";"):
            continue
        if re.match(r"^[\w.$-]+:", line):
            stats.basic_blocks += 1
            continue
        stats.ir_instructions += 1
        if m := IR_CALL.search(line):
            if not m.group(1).startswith("llvm."):
                stats.calls.append(m.group(1).strip('"'))
        if re.search(r"\b(invoke|landingpad|resume)\b", line) or any(
            marker in line for marker in UNWIND_MARKERS
        ):
            stats.unwind += 1
    return stats


def _asm_stats(asm: str) -> dict[str, list[str]]:
    function_names = re.findall(r"^\t\.type\t([\w.$]+),@function", asm, re.MULTILINE)
    begin = re.compile(r"^(" + "|".join(map(re.escape, function_names)) + r"):")
    functions = _split_functions(asm, begin, re.compile(r"^\.Lfunc_end\d+:"))
    return {
        name: [
            line.strip()
            for line in body
            if line.startswith("\t") and not line.strip().startswith((".", "#"))
        ]
        for name, body in functions.items()
    }


def _mca_rthroughput(llvm_bin: pathlib.Path, asm_lines: list[str]) -> float | None:
    result = sp.run(
        [_tool(llvm_bin, "llvm-mca"), "-mcpu=native"],
        input="\n".join(asm_lines),
        capture_output=True,
        text=True,
    )
    if m := re.search(r"Block RThroughput:\s*([\d.]+)", result.stdout):
        return float(m.group(1))
    return None


def collect(
    build_dir: pathlib.Path,
    sources: list[pathlib.Path],
    variant: str,
    llvm_bin: pathlib.Path,
    extra_flags: list[str],
    link: bool,
    name_filter: re.Pattern,
    mca: bool,
) -> dict[str, FunctionStats]:
    with tempfile.TemporaryDirectory() as tmp_str:
        tmp = pathlib.Path(tmp_str)
        bitcodes = []
        for i, source in enumerate(sources):
            out = tmp / f"{i}.bc"
            _run(
                [_tool(llvm_bin, "clang++")]
                + _compile_flags(build_dir, source, variant)
                + ["-O3", "-DNDEBUG", "-emit-llvm", "-c", "-o", str(out), str(source)]
                + extra_flags
            )
            bitcodes.append(out)

        modules = bitcodes
        if link:
            # Emulates LTO: handlers and dispatchers live in separate translation units.
            linked = tmp / "linked.bc"
            _run([_tool(llvm_bin, "llvm-link")] + [str(b) for b in bitcodes] + ["-o", str(linked)])
            optimized = tmp / "optimized.bc"
            _run([_tool(llvm_bin, "opt"), "-passes=default<O3>", str(linked), "-o", str(optimized)])
            modules = [optimized]

        results: dict[str, FunctionStats] = {}
        for module in modules:
            ir = _run([_tool(llvm_bin, "llvm-dis"), str(module), "-o", "-"])
            asm = _asm_stats(_run([_tool(llvm_bin, "llc"), "-O3", str(module), "-o", "-"]))

            mangled_names = [m.group(1).strip('"') for m in map(IR_DEFINE.match, ir.splitlines()) if m]
            demangled = _demangle(llvm_bin, mangled_names)
            for mangled in mangled_names:
                m = name_filter.search(demangled[mangled])
                if not m:
                    continue
                short = next(g for g in m.groups() if g) if m.groups() else m.group(0)
                stats = _ir_stats(ir, mangled)
                stats.name = short
                stats.calls = sorted(set(_demangle(llvm_bin, stats.calls).values())) if stats.calls else []
                if mangled in asm:
                    stats.x86_instructions = len(asm[mangled])
                    if mca:
                        stats.rthroughput = _mca_rthroughput(llvm_bin, asm[mangled])
                results[short] = stats

        obj_sizes = {}
        for module in modules:
            obj = module.with_suffix(".o")
            _run([_tool(llvm_bin, "llc"), "-O3", "-filetype=obj", "--function-sections", str(module), "-o", str(obj)])
            for line in _run([_tool(llvm_bin, "llvm-nm"), "-S", "-C", "--defined-only", str(obj)]).splitlines():
                parts = line.split(maxsplit=3)
                if len(parts) == 4 and (m := name_filter.search(parts[3])):
                    obj_sizes[next(g for g in m.groups() if g) if m.groups() else m.group(0)] = int(parts[1], 16)
        for name, size in obj_sizes.items():
            if name in results:
                results[name].bytes = size
        return results


def _fmt_delta(new: float, old: float | None) -> str:
    if old is None or new == old:
        return f"{new:g}"
    return f"{new:g} ({new - old:+g})"


def print_table(variant: str, stats: dict[str, FunctionStats], baseline: dict | None, show_calls: bool) -> None:
    columns = ["ir", "bbs", "x86", "bytes", "rthr", "calls", "unwind"]
    click.echo(click.style(f"\n[{variant}]", bold=True))
    click.echo(f"{'function':<40}" + "".join(f"{c:>14}" for c in columns))
    for name in sorted(stats):
        s = stats[name]
        old = (baseline or {}).get(name)
        values = [
            (s.ir_instructions, "ir_instructions"),
            (s.basic_blocks, "basic_blocks"),
            (s.x86_instructions, "x86_instructions"),
            (s.bytes, "bytes"),
            (s.rthroughput, "rthroughput"),
            (len(s.calls), None),
            (s.unwind, "unwind"),
        ]
        cells = []
        for value, key in values:
            if value is None:
                cells.append("-")
                continue
            cells.append(_fmt_delta(value, old.get(key) if old and key else None))
        line = f"{name:<40}" + "".join(f"{c:>14}" for c in cells)
        click.echo(click.style(line, fg="red") if s.unwind else line)
        if show_calls:
            for callee in s.calls:
                click.echo(f"    -> {callee}")


def ir_stats_impl(
    build_dir: str,
    source: tuple[str, ...],
    variant: tuple[str, ...],
    llvm_bin: str,
    extra_flag: tuple[str, ...],
    no_link: bool,
    filter: str,
    mca: bool,
    calls: bool,
    json_out: str | None,
    compare: str | None,
    check_no_unwind: bool,
) -> None:
    sources = [pathlib.Path(s) for s in (source or DEFAULT_SOURCES)]
    name_filter = re.compile(filter)
    baseline = json.loads(pathlib.Path(compare).read_text()) if compare else {}

    report: dict[str, dict[str, dict]] = {}
    unwinding: list[str] = []
    for v in variant or ("tc", "sc"):
        stats = collect(
            pathlib.Path(build_dir),
            sources,
            v,
            pathlib.Path(llvm_bin),
            list(extra_flag),
            not no_link,
            name_filter,
            mca,
        )
        print_table(v, stats, baseline.get(v), calls)
        report[v] = {name: asdict(s) for name, s in stats.items()}
        unwinding += [f"{v}:{name}" for name, s in stats.items() if s.unwind]

    if json_out:
        pathlib.Path(json_out).write_text(json.dumps(report, indent=2, sort_keys=True))

    if check_no_unwind and unwinding:
        exit_with_error("Unwinding code found in: " + ", ".join(unwinding))
