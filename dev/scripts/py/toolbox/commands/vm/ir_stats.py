# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ...impl.vm.ir_stats import DEFAULT_FILTER, DEFAULT_LLVM_BIN, ir_stats_impl
from click import Choice, Path, command, option


@command("ir-stats")
@option(
    "-b",
    "--build-dir",
    type=Path(exists=True, file_okay=False),
    default="build-release",
    show_default=True,
    help="Build directory whose compile_commands.json provides include paths and definitions.",
)
@option(
    "-s",
    "--source",
    multiple=True,
    type=Path(exists=True, dir_okay=False),
    help="Translation unit to analyse, repeatable. Defaults to the fast-mode handlers and dispatchers.",
)
@option(
    "-v",
    "--variant",
    multiple=True,
    type=Choice(["tc", "sc"]),
    help="Dispatch variant, repeatable. Defaults to both.",
)
@option(
    "--llvm-bin",
    type=Path(exists=True, file_okay=False),
    default=str(DEFAULT_LLVM_BIN),
    show_default=True,
    help="Directory with clang++, opt, llc, llvm-link, llvm-dis, llvm-nm, llvm-cxxfilt and llvm-mca.",
)
@option("-X", "--extra-flag", multiple=True, help="Extra clang flag, repeatable, ex. -X-fno-exceptions.")
@option(
    "--no-link",
    is_flag=True,
    default=False,
    help="Analyse every translation unit separately instead of linking and optimising them together.",
)
@option(
    "--from-build",
    is_flag=True,
    default=False,
    help="Disassemble the object files of --build-dir instead of compiling with clang, so the counts match "
    "the built binary. Only x86, bytes, rthr and calls are reported.",
)
@option(
    "-f",
    "--filter",
    default=DEFAULT_FILTER,
    show_default=True,
    help="Regex matched against demangled names; the first non-empty group is the reported name.",
)
@option("--mca", is_flag=True, default=False, help="Estimate block reciprocal throughput with llvm-mca.")
@option("--calls", is_flag=True, default=False, help="List the calls left in every function.")
@option("--json", "json_out", type=Path(dir_okay=False), help="Save the report as JSON.")
@option("--compare", type=Path(exists=True, dir_okay=False), help="JSON report to print deltas against.")
@option(
    "--check-no-unwind",
    is_flag=True,
    default=False,
    help="Fail if any reported function can unwind: it contains invoke/landingpad/resume, throws, or "
    "directly calls a function LLVM does not know to be nounwind.",
)
def ir_stats(*args, **kwargs):
    """
    Reports per-instruction code statistics of the fast-mode VM handlers.

    Every handler is compiled with the repository's clang at -O3, and for each one the
    command prints the LLVM IR instruction and basic block counts, the x86 instruction
    count and byte size, the calls that were not inlined, and the places an exception
    can unwind through. With --from-build the object files of the build are disassembled
    instead. Use --json and --compare to track changes between revisions.
    """
    ir_stats_impl(*args, **kwargs)
