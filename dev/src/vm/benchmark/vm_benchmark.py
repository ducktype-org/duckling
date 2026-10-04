#!/usr/bin/env python3

import click
import shlex
import subprocess
import os
import random
from time import sleep
import time
import datetime

# Extensions:
BC_EXTENSION = ".dbc"
INPUT_FILE_EXTENSION = ".in"
CSV_EXTENSION = ".csv"

# Directories:
VM_BINARIES_DIR = "vm_bins"
BUILD_DIR_PATH_TO_BINARIES = "./bin/"

# Compile options:
DEFAULT_VM_TARGET = "VM"
DEFAULT_CPU_CORES = os.cpu_count() or 1


def log_error(message):
    click.echo(click.style(message, fg="red", bold=True))


def log_success(message):
    click.echo(click.style(message, fg="green", bold=True))


def log_info(message):
    click.echo(click.style(message, fg="white", bold=True))


def log_text(message):
    click.echo(click.style(message, fg="white", bold=False))


@click.group()
def cli():
    """
    The script to run benchmarks on VM executables.

    The 'compile' command compiles the VM executable
    and places it in the binaries directory.
    The 'run' command runs benchmarks for all binaries in the binaries directory.
    The 'results' command displays results for the given benchmark suite.
    """
    pass


@cli.command(no_args_is_help=True)
@click.argument(
    "path_to_cmake",
    type=click.Path(exists=True),
    required=True,
)
@click.argument(
    "cmake_target",
    default=DEFAULT_VM_TARGET,
    type=click.STRING,
)
@click.option(
    "-j",
    "--cpu_cores",
    default=DEFAULT_CPU_CORES,
    help="Number of CPU cores to use for compilation.",
)
@click.option(
    "--suffix",
    type=click.STRING,
    help="If specified, in the compiled binary name suffix will be appended.",
)
@click.option(
    "--build_dir",
    default="build",
    type=click.STRING,
    help="Name of the build directory.",
)
@click.option(
    "--binaries_dir",
    default=VM_BINARIES_DIR,
    type=click.STRING,
    help="Name of the directory where VM binaries will be stored.",
)
@click.option(
    "--cxx_compiler",
    type=click.STRING,
    help="Path to the C++ compiler.",
)
@click.option(
    "--cmake_options",
    type=click.STRING,
    help="Additional options for CMake.",
)
def compile(
    path_to_cmake,
    cmake_target,
    cpu_cores,
    suffix,
    build_dir,
    binaries_dir,
    cxx_compiler,
    cmake_options,
):
    """
    The script compiles the CMAKE_TARGET in Release. It uses the CMakeLists.txt file
    located in the PATH_TO_CMAKE. The compiled binary is renamed and placed in the BINARIES_DIR.
    """
    create_dir_if_not_exists(binaries_dir)

    try:
        bin_path = cmake_build_target(
            path_to_cmake,
            build_dir,
            cmake_target,
            cpu_cores,
            cxx_compiler,
            cmake_options,
        )

    except subprocess.CalledProcessError as err:
        # If return code is non-zero, raise a CalledProcessError and print this message
        log_error("Compilation error...")
        return

    [branch, commit] = get_git_head_info()
    exe_file_name = f"{cmake_target}_{branch}_{commit}"

    # Append compiler name to the binary name if provided
    if cxx_compiler and "clang" in cxx_compiler:
        exe_file_name += "_clang"
    elif cxx_compiler and ("g++" in cxx_compiler or "gcc" in cxx_compiler):
        exe_file_name += "_gcc"

    # Append suffix to the binary name if provided
    if suffix:
        exe_file_name += f"_{suffix}"

    new_executable_path = os.path.join(binaries_dir, exe_file_name)

    while os.path.exists(new_executable_path):
        new_executable_path += datetime.datetime.now().strftime("_%y%m%d%H%M%S")

    subprocess.run(["mv", bin_path, new_executable_path])

    log_success("Compiled binary to " + new_executable_path)


@cli.command(no_args_is_help=True)
@click.argument(
    "benchmark_suite",
    type=click.STRING,
)
@click.option(
    "--only",
    help="If specified, only benchmark binary the given binary path.",
    type=click.STRING,
)
@click.option(
    "--binaries_dir",
    default=VM_BINARIES_DIR,
    type=click.Path(),
    help="Name of the directory where VM binaries will be stored.",
)
@click.option(
    "--programs_dir",
    default="benchmark_programs",
    type=click.Path(exists=True),
    help="Name of the directory where benchmark programs are stored.",
)
@click.option(
    "--inputs_dir",
    default="benchmark_inputs",
    type=click.Path(exists=True),
    help="Name of the directory where benchmark program's inputs are stored.",
)
@click.option(
    "--results_dir",
    default="results",
    type=click.Path(),
    help="Name of the directory where benchmark results will be stored.",
)
@click.option(
    "--reps",
    default=5,
    type=click.INT,
    help="Number of repetitions for each benchmark.",
)
@click.option(
    "--program",
    default=None,
    type=click.STRING,
    help="Name of the program file to benchmark.",
    multiple=True,
)
@click.pass_context
def run(
    ctx,
    benchmark_suite,
    only,
    binaries_dir,
    programs_dir,
    inputs_dir,
    results_dir,
    reps,
    program,
):
    """
    Runs benchmarks for all binaries in the BINARIES_DIR directory
    on the BENCHMARK_SUITE. The results for each VM executable
    are stored in the RESULTS_DIR in separate CSV files. You can
    specify the number of repetitions for each benchmark with the REPS option.
    Example usage:

        python3 vm_benchmark.py run fast --reps 5

    To run benchmarks only for the given binary, use the --only option:

        python3 vm_benchmark.py run fast --only <path_to_binary>

    You can also narrow the programs to benchmark with --program option (multiple allowed):

        python3 vm_benchmark.py run fast --program collatz.dbc --program fib.dbc

    """
    create_dir_if_not_exists(results_dir)

    if only:
        binaries = [only]
    else:
        binaries = get_all_files_in_dir(binaries_dir)
        if len(binaries) == 0:
            log_info(
                "You may want to compile the binary first with command:\n"
                "    python3 vm_benchmark.py compile <path_to_cmake_directory>"
            )
            return

    benchmark_suite_files = get_benchmark_suite_files(
        programs_dir, inputs_dir, benchmark_suite, program
    )

    log_info("Running benchmark on binaries:")
    for binary in binaries:
        log_info("  - " + binary)
    log_info("")

    log_info("Benchmark suite:")
    if len(benchmark_suite_files) > 0:

        for program, _ in benchmark_suite_files:
            log_info("  - " + program)
        log_info("")

        for binary in binaries:
            vm_benchmark_one(
                binary, benchmark_suite_files, results_dir, benchmark_suite, reps
            )
            log_info("")

        ctx.invoke(results, benchmark_suite=benchmark_suite, results_dir=results_dir)
    else:
        log_info("No benchmark programs found.")


@cli.command(no_args_is_help=True)
@click.argument(
    "benchmark_suite",
    type=click.STRING,
)
@click.option(
    "--results_dir",
    default="results",
    type=click.Path(),
    help="Name of the directory where benchmark results are stored.",
)
def results(benchmark_suite, results_dir):
    """
    Displays results for the BENCHMARK_SUITE,
    from the CSV file stored in the RESULTS_DIR.
    """
    results = get_all_files_in_dir(results_dir, benchmark_suite + CSV_EXTENSION)

    data = []
    for result in results:
        program_name = os.path.basename(result)[
            : -(1 + len(CSV_EXTENSION) + len(benchmark_suite))
        ]
        file_data = list()

        with open(result, "r") as f:
            lines = f.readlines()

            for line in lines:
                elems = line.split(",")
                elems = elems[:-1]
                elems = [elem.strip() for elem in elems]
                test_name = elems[0]
                test_input = elems[1]
                test_runs = elems[2:]
                file_data.append((test_name, test_input, test_runs))

        data.append((program_name, sorted(file_data)))

    # for every program and every time calculate minimum
    max_test_input_len = 10
    processed_data = []
    for program_name, file_data in data:
        processed_file = list()
        for test_name, test_input, test_runs in file_data:
            test_runs = [float(run) for run in test_runs]
            if len(test_input) > max_test_input_len:
                max_test_input_len = len(test_input)
            processed_file.append((test_name, test_input, min(test_runs)))

        processed_data.append((program_name, processed_file))

    # display results for every program
    log_info(f"\nResults for '{benchmark_suite}' benchmark suite:")
    log_text(
        " " * 4
        + "test name".ljust(25)
        + " "
        + "input".ljust(max_test_input_len + 1)
        + " "
        + "t_min [s]".rjust(10)
        + "\n"
    )
    for program, results in processed_data:
        log_info("--- " + program + ":")
        for test_name, test_input, test_result in results:
            log_text(
                " " * 4
                + test_name.ljust(25)
                + " "
                + test_input.ljust(max_test_input_len + 1)
                + " "
                + time_or_dnf(test_result).rjust(10)
            )
        log_text("")


def cmake_build_target(
    cmake_path, build_dir, vm_target, cpu_cores, cxx_compiler, cmake_options
):
    """Builds the project with CMake.
    Path to cmake should be relative to the current directory.
    Returns absolute path to the compiled binary."""
    current_directory = os.getcwd()
    os.chdir(cmake_path)

    run_cmake_command = [
        "cmake",
        "-S",
        ".",
        "-B",
        build_dir,
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    if cxx_compiler:
        run_cmake_command += ["-DCMAKE_CXX_COMPILER=" + cxx_compiler]
    if cmake_options:
        run_cmake_command += shlex.split(cmake_options)

    log_info(shlex.join(run_cmake_command))
    subprocess.run(run_cmake_command)

    compile_target_command = [
        "cmake",
        "--build",
        build_dir,
        "--target",
        vm_target,
    ]
    if cpu_cores > 1:
        compile_target_command += ["-j", str(cpu_cores)]

    log_info(shlex.join(compile_target_command))
    result = subprocess.run(compile_target_command)
    result.check_returncode()

    vm_bin_abs_path = os.path.abspath(
        os.path.join(build_dir, BUILD_DIR_PATH_TO_BINARIES, vm_target)
    )
    os.chdir(current_directory)
    return vm_bin_abs_path


def run_command_with_time(command: list, stdin: str):
    env_vars = {"TIMEFORMAT": "%R", "LC_NUMERIC": "en_US.UTF-8"}
    prefix = ["time", "-f", "%U"]
    full_command = prefix + command
    return subprocess.run(
        full_command, input=stdin, capture_output=True, text=True, env=env_vars
    )


def run_vm(vm_bin_path, program_file, stdin_str):
    result = run_command_with_time([vm_bin_path, "-f", program_file], stdin_str)

    stderr_lines = result.stderr.split("\n")
    measured_time = float(stderr_lines[-2])

    if len(stderr_lines) > 2:
        log_error(
            "Got abnormal stderr output.\n"
            + "Make sure binary does not print anything to stderr."
        )

        log_error("return code: " + str(result.returncode))
        log_error("stderr: " + result.stderr)
        measured_time = -1

    return [result.stdout, measured_time]


def time_or_dnf(measured_time):
    return str(measured_time) if measured_time > 0 else "DNF"


def get_benchmark_times(vm_bin_path, benchmark_program, stdin, repeats=5):
    times = []
    benchmark_program_name = os.path.basename(benchmark_program)
    display_stdin = stdin.replace("\n", " ").strip()
    log_text("-" * 60)
    log_info(benchmark_program_name[:30].ljust(30) + display_stdin[:30].rjust(30))

    for i in range(repeats):
        [_, time] = run_vm(vm_bin_path, benchmark_program, stdin)
        times.append(time)

        log_text(" " * 4 + time_or_dnf(time))

        sleep(random.uniform(0, 0.5))

    return times


def get_git_head_info():
    result = subprocess.run(
        ["git", "branch", "--show-current"], capture_output=True, text=True
    )
    branch_name = result.stdout.strip()
    if branch_name == "":
        branch_name = "detached"
    branch_name = branch_name.replace("/", "_")
    branch_name = branch_name.replace("\\", "_")

    result = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True
    )
    commit_hash = result.stdout.strip()

    return [branch_name, commit_hash]


def get_stdin_list(path_to_input):
    data = ""
    with open(path_to_input, "r") as f:
        data = f.read()

    return data.split("\n\n")


def create_dir_if_not_exists(dir_name):
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)
        log_info(f"Created directory: {dir_name}")


def save_benchmark_results(raport_path, results):
    with open(raport_path, "w") as f:
        for program, stdin, times in results:
            f.write((program + ",").ljust(20))
            f.write((str(stdin).replace("\n", " ") + ",").ljust(15))
            for exec_time in times:
                f.write((str(exec_time) + ",").ljust(10))
            f.write("\n")

    log_success("Results saved to: " + raport_path)


def vm_benchmark_one(vm_bin_path, benchmark_files, results_dir, inputs_suite, reps):
    binary_name = os.path.basename(vm_bin_path)
    raport_path = os.path.join(
        results_dir, f"{binary_name}_{inputs_suite}" + CSV_EXTENSION
    )
    results = list()

    log_info("Running benchmark for: " + binary_name)

    start = time.time()

    for program_file_path, input_file_path in benchmark_files:
        for stdin in get_stdin_list(input_file_path):
            stdin_times = get_benchmark_times(
                vm_bin_path, program_file_path, stdin, repeats=reps
            )
            program = os.path.basename(program_file_path)
            results.append((program, stdin, stdin_times))

    end = time.time()
    log_info(f"Elapsed time: " + str(datetime.timedelta(seconds=int(end - start))))

    save_benchmark_results(raport_path, results)

    return results


def get_all_files_in_dir(dir_name, extension=None):
    """Returns list of paths to all files in a given directory
    with given extension."""
    if not os.path.exists(dir_name):
        log_error(f"Directory {dir_name} does not exist.")
        return []

    files = []
    with os.scandir(dir_name) as it:
        for entry in it:
            if entry.is_file() and (not extension or entry.name.endswith(extension)):
                files.append(os.path.join(dir_name, entry.name))
    return sorted(files)


def get_benchmark_suite_files(
    programs_dir, inputs_dir, input_suite_name, programs_only
):
    """
    Find's all input files in the inputs_dir and matches them with
    programs in the programs_dir. The name of the input file should
    match the name of the program file (without extensions).
    Returns a list of pairs: (benchmark_program_path, benchmark_input_path),
    where path's are relative to the directory of the script.

    Argument `programs_only` should be a list of program names (ex. 'collatz.dbc').
    If `programs_only` is provided, only programs from the list will be benchmarked.
    """
    benchmark_suite_files = (
        []
    )  # list of pairs: (benchmark_program_path, benchmark_input_path)
    benchmark_inputs = get_all_files_in_dir(
        os.path.join(inputs_dir, input_suite_name), INPUT_FILE_EXTENSION
    )
    if programs_only and len(programs_only) > 0:
        all_benchmark_programs = [
            os.path.join(programs_dir, code) for code in programs_only
        ]
    else:
        all_benchmark_programs = get_all_files_in_dir(programs_dir, BC_EXTENSION)

    for input_path in benchmark_inputs:
        name = os.path.basename(input_path)[: -len(INPUT_FILE_EXTENSION)]

        for program_path in all_benchmark_programs:
            program_name = os.path.basename(program_path)[: -len(BC_EXTENSION)]
            if program_name == name:
                benchmark_suite_files.append((program_path, input_path))

    return benchmark_suite_files


if __name__ == "__main__":
    cli()
