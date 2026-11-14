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
    "src/vm/src/vm/core/fast/eval/helpers.cpp",
    "src/vm/src/vm/core/fast/program/instructions/executable.cpp",
]
DEFAULT_FILTER = r"FastExecutor::(Instr_\w+|eval)\b|\b(dispatcher_\w+)|\b(stencil_\w+)|(FastHelpers::\w+)"
# Helpers may contain exception handling, but every exception must end in their catch-all.
HELPER_PREFIX = "FastHelpers::"
# Only the dispatch functions may call indirectly: the next instruction's dispatcher.
DISPATCH_NAMES = re.compile(r"^(eval|dispatcher_\w+)$")
# Ending a catch block may throw, which clang routes to terminate; that is not an escape.
END_CATCH = "__cxa_end_catch"
VARIANT_DEFINES = {"tc": "-DUSE_TAIL_CALLS", "sc": "-DUSE_SWITCH_CASE"}
UNWIND_MARKERS = ("__cxa_throw", "__cxa_rethrow", "_Unwind_Resume")


@dataclass
class FunctionStats:
    name: str
    ir_instructions: int | None = 0
    basic_blocks: int | None = 0
    calls: list[str] = field(default_factory=list)
    # Ways an exception can leave the function, see `--check-no-unwind`.
    unwind: int | None = 0
    may_unwind_calls: list[str] = field(default_factory=list)
    x86_instructions: int | None = 0
    bytes: int | None = 0
    rthroughput: float | None = None


def _tool(llvm_bin: pathlib.Path, name: str) -> str:
    path = llvm_bin / name
    if not path.exists():
        exit_with_error(f"`{path}` does not exist, run `./toolbox.py install-llvm` or pass --llvm-bin")
    return str(path)


def _run(cmd: list[str], input: str | None = None) -> str:
    result = sp.run(cmd, capture_output=True, text=True, input=input)
    if result.returncode != 0:
        exit_with_error(f"`{shlex.join(cmd[:3])} ...` failed:\n{result.stderr}")
    return result.stdout


def _compile_command(build_dir: pathlib.Path, source: pathlib.Path, variant: str) -> tuple[list[str], pathlib.Path]:
    """Returns the compiler invocation of `source` from the build's compile_commands.json and its object file."""
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
        output = entry.get("output") or tokens[tokens.index("-o") + 1]
        return tokens, pathlib.Path(entry["directory"]) / output

    exit_with_error(f"No `{variant}` compile command for `{source}` in `{db_path}`")


def _compile_flags(build_dir: pathlib.Path, source: pathlib.Path, variant: str) -> list[str]:
    """Picks the include paths and definitions of `source` from the build's compile_commands.json."""
    tokens, _ = _compile_command(build_dir, source, variant)
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
IR_FUNCTION = re.compile(r'^(?:define|declare) [^@]*@("?[^"(]+"?)\(')
IR_CALL = re.compile(r'\b(?:call|invoke)\b[^@]*@("?[\w.$]+"?)\(')
IR_INDIRECT_CALL = re.compile(r"\b(?:call|invoke)\b[^@(]*\s%[\w.$-]+\(")
INDIRECT_CALL = "<indirect call>"
IR_ATTRIBUTE_GROUP = re.compile(r"^attributes #(\d+) = \{(.*)\}")


def _nounwind_functions(ir: str) -> tuple[set[str], set[str]]:
    """Names of the functions LLVM marked or inferred `nounwind`, and the `nounwind` attribute groups."""
    nounwind_groups = {
        m.group(1)
        for m in map(IR_ATTRIBUTE_GROUP.match, ir.splitlines())
        if m and re.search(r"\bnounwind\b", m.group(2))
    }
    result = set()
    for line in ir.splitlines():
        if m := IR_FUNCTION.match(line):
            attributes = line[m.end() :]
            if re.search(r"\bnounwind\b", attributes) or nounwind_groups & set(re.findall(r"#(\d+)", attributes)):
                result.add(m.group(1).strip('"'))
    return result, nounwind_groups


def _ir_stats(body: list[str], nounwind: tuple[set[str], set[str]]) -> tuple[FunctionStats, int]:
    """
    Counts the function's instructions and its calls. Calls that may unwind, i.e. to functions not
    known to be `nounwind`, go to `may_unwind_calls`; the returned count is the number of
    `invoke`, `landingpad` and `resume` instructions and of throws.
    """
    nounwind_functions, nounwind_groups = nounwind
    stats = FunctionStats(name="")
    unwinding_instructions = 0
    stats.basic_blocks = 1
    for line in body:
        line = line.strip()
        if not line or line.startswith(";"):
            continue
        if re.match(r"^[\w.$-]+:", line):
            stats.basic_blocks += 1
            continue
        stats.ir_instructions += 1
        if re.search(r"\b(invoke|landingpad|resume)\b", line) or any(
            marker in line for marker in UNWIND_MARKERS
        ):
            unwinding_instructions += 1
            if m := IR_CALL.search(line):
                stats.calls.append(m.group(1).strip('"'))
            continue

        if (m := IR_CALL.search(line)) and not m.group(1).startswith("llvm."):
            callee = m.group(1).strip('"')
            stats.calls.append(callee)
            call_site_groups = set(re.findall(r"#(\d+)", line[m.end() :]))
            if callee not in nounwind_functions and not (call_site_groups & nounwind_groups):
                stats.may_unwind_calls.append(callee)
        elif m := IR_INDIRECT_CALL.search(line):
            call_site_groups = set(re.findall(r"#(\d+)", line[m.end() :]))
            if not (call_site_groups & nounwind_groups) and not re.search(r"\bnounwind\b", line):
                stats.may_unwind_calls.append(INDIRECT_CALL)
    return stats, unwinding_instructions


def _escaping_invokes(body: list[str]) -> list[str]:
    """Callees of the `invoke`s whose exceptions are not caught by a catch-all landing pad."""
    blocks: dict[str, list[str]] = {}
    current = blocks.setdefault("", [])
    for line in body:
        if m := re.match(r"^([\w.$-]+):", line):
            current = blocks.setdefault(m.group(1), [])
        else:
            current.append(line.strip())

    def catches_all(label: str) -> bool:
        # clang's terminate pad of a `noexcept` function is a catch-all that terminates.
        block = blocks.get(label, [])
        return any(line.startswith("catch ptr null") for line in block) and not any(
            "__clang_call_terminate" in line or "@_ZSt9terminatev" in line for line in block
        )

    escaping = []
    lines = [line.strip() for line in body]
    for i, line in enumerate(lines):
        if line.startswith("resume "):
            escaping.append("<resume>")
            continue
        if not (m := re.search(r"\binvoke\b[^@(]*\s(@\"?[\w.$]+\"?|%[\w.$-]+)\(", line)):
            continue
        callee = m.group(1).lstrip("@").strip('"') if m.group(1).startswith("@") else INDIRECT_CALL
        unwind = re.search(r"unwind label %([\w.$-]+)", line + " " + (lines[i + 1] if i + 1 < len(lines) else ""))
        if callee != END_CATCH and not (unwind and catches_all(unwind.group(1))):
            escaping.append(callee)
    return escaping


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
    ir_only: bool = False,
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
            asm = {} if ir_only else _asm_stats(_run([_tool(llvm_bin, "llc"), "-O3", str(module), "-o", "-"]))

            bodies = _split_functions(ir, IR_DEFINE, re.compile(r"^}"))
            demangled = _demangle(llvm_bin, list(bodies))
            nounwind = _nounwind_functions(ir)
            selected: list[tuple[str, FunctionStats]] = []
            for mangled, body in bodies.items():
                m = name_filter.search(demangled[mangled])
                if not m:
                    continue
                stats, unwinding_instructions = _ir_stats(body, nounwind)
                stats.name = next(g for g in m.groups() if g) if m.groups() else m.group(0)
                if stats.name.startswith(HELPER_PREFIX):
                    # A helper may handle exceptions; only those its catch-all misses escape.
                    stats.may_unwind_calls += _escaping_invokes(body)
                    stats.unwind = len(stats.may_unwind_calls)
                else:
                    if DISPATCH_NAMES.match(stats.name):
                        stats.may_unwind_calls = [c for c in stats.may_unwind_calls if c != INDIRECT_CALL]
                    stats.unwind = unwinding_instructions + len(stats.may_unwind_calls)
                selected.append((mangled, stats))

            callee_names = sorted({c for _, s in selected for c in s.calls + s.may_unwind_calls} - {INDIRECT_CALL})
            callees = _demangle(llvm_bin, callee_names) if callee_names else {}
            for mangled, stats in selected:
                short = stats.name
                stats.calls = sorted({callees.get(c, c) for c in stats.calls})
                stats.may_unwind_calls = sorted({callees.get(c, c) for c in stats.may_unwind_calls})
                if ir_only:
                    stats.x86_instructions = stats.bytes = None
                elif mangled in asm:
                    stats.x86_instructions = len(asm[mangled])
                    if mca:
                        stats.rthroughput = _mca_rthroughput(llvm_bin, asm[mangled])
                results[short] = stats

        obj_sizes = {}
        for module in [] if ir_only else modules:
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


OBJDUMP_FUNCTION = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
OBJDUMP_INSTRUCTION = re.compile(r"^\s*[0-9a-f]+:\s+(\S+)\s*(.*)$")
OBJDUMP_RELOCATION = re.compile(r"^\s*[0-9a-f]+:\s+R_X86_64_\w+\s+([^\s+-]+)")


def collect_from_build(
    build_dir: pathlib.Path,
    sources: list[pathlib.Path],
    variant: str,
    llvm_bin: pathlib.Path,
    name_filter: re.Pattern,
    mca: bool,
) -> dict[str, FunctionStats]:
    """Disassembles the object files the build produced, so the counts are the ones of the shipped binary."""
    results: dict[str, FunctionStats] = {}
    for source in sources:
        _, obj = _compile_command(build_dir, source, variant)
        if not obj.exists():
            exit_with_error(f"`{obj}` does not exist, build `{build_dir}` first")

        disassembly = _run([_tool(llvm_bin, "llvm-objdump"), "-d", "-r", "--no-show-raw-insn", str(obj)])
        functions: dict[str, list[tuple[str, str, str | None]]] = {}
        current: list | None = None
        for line in disassembly.splitlines():
            if m := OBJDUMP_FUNCTION.match(line):
                # GCC moves cold paths to `<name>.cold`; count them with their function, like LLVM does.
                current = functions.setdefault(m.group(1).removesuffix(".cold"), [])
            elif current is not None and (m := OBJDUMP_RELOCATION.match(line)):
                if current:
                    mnemonic, operands, _ = current[-1]
                    current[-1] = (mnemonic, operands, m.group(1))
            elif current is not None and (m := OBJDUMP_INSTRUCTION.match(line)):
                current.append((m.group(1), m.group(2), None))

        sizes: dict[str, int] = {}
        for line in _run([_tool(llvm_bin, "llvm-nm"), "-S", "--defined-only", str(obj)]).splitlines():
            parts = line.split()
            if len(parts) == 4:
                name = parts[3].removesuffix(".cold")
                sizes[name] = sizes.get(name, 0) + int(parts[1], 16)

        demangled = _demangle(llvm_bin, list(functions))
        for mangled, instructions in functions.items():
            m = name_filter.search(demangled[mangled])
            if not m:
                continue
            short = next(g for g in m.groups() if g) if m.groups() else m.group(0)
            callees = {
                target
                for mnemonic, _, target in instructions
                if mnemonic.startswith(("call", "jmp")) and target and not target.startswith(".")
            }
            stats = FunctionStats(
                name=short,
                ir_instructions=None,
                basic_blocks=None,
                unwind=None,
                calls=sorted(set(_demangle(llvm_bin, sorted(callees)).values())) if callees else [],
                x86_instructions=len(instructions),
                bytes=sizes.get(mangled, 0),
            )
            if mca:
                stats.rthroughput = _mca_rthroughput(
                    llvm_bin, [f"{mn} {re.sub(r'<[^>]*>', '', ops)}" for mn, ops, _ in instructions]
                )
            results[short] = stats
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
            for callee in sorted(set(s.calls) | set(s.may_unwind_calls)):
                suffix = "  (may unwind)" if callee in s.may_unwind_calls else ""
                click.echo(f"    -> {callee}{suffix}")


def ir_stats_impl(
    build_dir: str,
    source: tuple[str, ...],
    variant: tuple[str, ...],
    llvm_bin: str,
    extra_flag: tuple[str, ...],
    no_link: bool,
    from_build: bool,
    ir_only: bool,
    filter: str,
    mca: bool,
    calls: bool,
    json_out: str | None,
    compare: str | None,
    check_no_unwind: bool,
) -> None:
    if from_build and ir_only:
        exit_with_error("--from-build and --ir-only exclude each other")
    if from_build and (check_no_unwind or extra_flag or no_link):
        exit_with_error("--from-build reads the built objects; it cannot be combined with -X, --no-link or --check-no-unwind")

    sources = [pathlib.Path(s) for s in (source or DEFAULT_SOURCES)]
    name_filter = re.compile(filter)
    baseline = json.loads(pathlib.Path(compare).read_text()) if compare else {}

    report: dict[str, dict[str, dict]] = {}
    unwinding: list[str] = []
    for v in variant or ("tc", "sc"):
        if from_build:
            stats = collect_from_build(pathlib.Path(build_dir), sources, v, pathlib.Path(llvm_bin), name_filter, mca)
        else:
            stats = collect(
                pathlib.Path(build_dir),
                sources,
                v,
                pathlib.Path(llvm_bin),
                list(extra_flag),
                not no_link,
                name_filter,
                mca and not ir_only,
                ir_only,
            )
        print_table(v, stats, baseline.get(v), calls)
        report[v] = {name: asdict(s) for name, s in stats.items()}
        unwinding += [f"{v}:{name}" for name, s in stats.items() if s.unwind]

    if json_out:
        pathlib.Path(json_out).write_text(json.dumps(report, indent=2, sort_keys=True))

    if check_no_unwind and unwinding:
        exit_with_error("Unwinding code found in: " + ", ".join(unwinding))
