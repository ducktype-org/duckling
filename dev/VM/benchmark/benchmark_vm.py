#!/usr/bin/env python3

import argparse
import subprocess
import os
import random
from time import sleep
import time
import datetime

# Extensions:
DUCKLING_BC_EXTENSION = ".dbc"
INPUT_FILE_EXTENSION = ".in"
CSV_EXTENSION = ".csv"

# Directories:
BUILD_DIR_NAME = "build"
BENCHMARK_INPUTS_DIR = "benchmark_inputs"
BENCHMARK_PROGRAMS_DIR = "benchmark_programs"
DUCKLING_VM_BINARIES_DIR = "duckling_vm_bins"
RESULTS_DIR = "results"
BUILD_DIR_PATH_TO_BINARIES = "./bin/"

# Benchmark suite:
DEFAULT_BENCHMARK_SUITE = "fast"
BENCHMARK_ONLY = []
# BENCHMARK_ONLY = ["ackermann.dbc"]

# Compile options:
DEAFULT_DUCKLING_VM_TARGET = "Vm"
DEFAULT_CPU_CORES = 1


class bcolors:
    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    GREEN = "\033[32m"
    YELLOW = "\33[33m"
    RED = "\033[31m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


def run_command_with_time(command: list, stdin: str):
    env_vars = {"TIMEFORMAT": "%R", "LC_NUMERIC": "en_US.UTF-8"}
    prefix = ["time", "-f", "%U"]
    full_command = prefix + command
    return subprocess.run(
        full_command, input=stdin, capture_output=True, text=True, env=env_vars
    )


def cmake_build_binary(cmake_path, vm_target, cpu_cores):
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
        BUILD_DIR_NAME,
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    subprocess.run(run_cmake_command)

    compile_target_command = [
        "cmake",
        "--build",
        BUILD_DIR_NAME,
        "--target",
        vm_target,
    ]
    if cpu_cores > 1:
        compile_target_command += ["-j", str(cpu_cores)]

    result = subprocess.run(compile_target_command)
    result.check_returncode()

    vm_bin_abs_path = os.path.abspath(
        os.path.join(BUILD_DIR_NAME, BUILD_DIR_PATH_TO_BINARIES, vm_target)
    )
    os.chdir(current_directory)
    return vm_bin_abs_path


def run_vm(duckling_vm_bin_path, program_file, stdin_str):
    result = run_command_with_time([duckling_vm_bin_path, "-f", program_file], stdin_str)

    stderr_lines = result.stderr.split("\n")
    measured_time = float(stderr_lines[-2])

    if len(stderr_lines) > 2:
        print(
            bcolors.RED
            + "Got abnormal stderr output.\n"
            + "Make sure binary does not print anything to stderr."
        )
        print("Return code: " + str(result.returncode))
        print("stderr: " + result.stderr + bcolors.ENDC)
        measured_time = -1

    return [result.stdout, measured_time]


def time_or_dnf(measured_time):
    return str(measured_time) if measured_time > 0 else "DNF"


def get_benchmark_times(duckling_vm_bin_path, benchmark_program, stdin, repeats=5):
    times = []
    benchmark_program_name = os.path.basename(benchmark_program)
    display_stdin = stdin.replace("\n", " ").strip()
    print("-" * 60)
    print(benchmark_program_name[:30].ljust(30) + display_stdin[:30].rjust(30))

    for i in range(repeats):
        [_, time] = run_vm(duckling_vm_bin_path, benchmark_program, stdin)
        times.append(time)

        print(" " * 4 + time_or_dnf(time))

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


def save_benchmark_results(raport_path, results):
    with open(raport_path, "w") as f:
        for program, stdin, times in results:
            f.write((program + ",").ljust(20))
            f.write((str(stdin).replace("\n", " ") + ",").ljust(15))
            for exec_time in times:
                f.write((str(exec_time) + ",").ljust(10))
            f.write("\n")

    print(bcolors.GREEN + f"Results saved to: {raport_path}" + bcolors.ENDC)


def benchmark_one_vm(duckling_vm_bin_path, benchmark_files, args):
    binary_name = os.path.basename(duckling_vm_bin_path)
    raport_path = os.path.join(
        args.results_dir, f"{binary_name}_{args.inputs_suite}" + CSV_EXTENSION
    )
    results = list()

    print(f"\nRunning benchmark for:")
    print(bcolors.HEADER + binary_name + bcolors.ENDC)

    start = time.time()

    for program_file_path, input_file_path in benchmark_files:
        for stdin in get_stdin_list(input_file_path):
            stdin_times = get_benchmark_times(
                duckling_vm_bin_path, program_file_path, stdin, repeats=args.reps
            )
            program = os.path.basename(program_file_path)
            results.append((program, stdin, stdin_times))

    end = time.time()
    print(f"Elapsed time: " + str(datetime.timedelta(seconds=int(end - start))))

    save_benchmark_results(raport_path, results)

    return results


def get_all_files_in_dir(dir_name, extension=None):
    """Returns list of paths to all files in a given directory
    with given extension."""
    if not os.path.exists(dir_name):
        print(bcolors.RED + f"Directory {dir_name} does not exist." + bcolors.ENDC)
        return []

    files = []
    with os.scandir(dir_name) as it:
        for entry in it:
            if entry.is_file() and (not extension or entry.name.endswith(extension)):
                files.append(os.path.join(dir_name, entry.name))
    return sorted(files)


def build_binary_and_save(args):
    """
    Builds the project with CMake and moves the binary to the binaries directory.
    Returns the path to the new binary.
    """
    create_dir_if_not_exists(args.binaries_dir)

    try:
        bin_path = cmake_build_binary(args.cmake, args.target, args.cpu_cores)

    except subprocess.CalledProcessError as err:
        # If return code is non-zero, raise a CalledProcessError and print this message
        print(bcolors.RED + "Compilation error..." + bcolors.ENDC)
        return

    [branch, commit] = get_git_head_info()
    new_bin_path = os.path.join(args.binaries_dir, f"{args.target}_{branch}_{commit}")

    if args.suffix:
        new_bin_path = new_bin_path + "_" + args.suffix

    while os.path.exists(new_bin_path):
        new_bin_path += datetime.datetime.now().strftime("_%y%m%d%H%M%S")

    subprocess.run(["mv", bin_path, new_bin_path])

    print(bcolors.GREEN + "Compiled binary to " + new_bin_path + bcolors.ENDC)


def get_benchmark_suite_files(
    programs_dir, inputs_dir, input_suite_name=DEFAULT_BENCHMARK_SUITE
):
    """
    Find's all input files in the inputs_dir and matches them with
    programs in the programs_dir. The name of the input file should
    match the name of the program file (without extensions).
    Returns a list of pairs: (benchmark_program_path, benchmark_input_path),
    where path's are relative to the directory of the script.

    If BENCHMARK_ONLY is specified, only benchmarks the given programs.
    """
    benchmark_suite_files = (
        []
    )  # list of pairs: (benchmark_program_path, benchmark_input_path)
    benchmark_inputs = get_all_files_in_dir(
        os.path.join(inputs_dir, input_suite_name), INPUT_FILE_EXTENSION
    )
    if BENCHMARK_ONLY and len(BENCHMARK_ONLY) > 0:
        all_benchmark_programs = [
            os.path.join(programs_dir, code) for code in BENCHMARK_ONLY
        ]
    else:
        all_benchmark_programs = get_all_files_in_dir(programs_dir, DUCKLING_BC_EXTENSION)

    for input_path in benchmark_inputs:
        name = os.path.basename(input_path)[: -len(INPUT_FILE_EXTENSION)]

        for program_path in all_benchmark_programs:
            program_name = os.path.basename(program_path)[: -len(DUCKLING_BC_EXTENSION)]
            if program_name == name:
                benchmark_suite_files.append((program_path, input_path))

    return benchmark_suite_files


def run_benchmarks(args):
    """
    If args.only_benchmark is specified, only benchmarks the given binary.
    Otherwise, benchmarks all binaries in the binaries directory.
    """

    if args.only:
        binaries = [args.only]
    else:
        binaries = get_all_files_in_dir(args.binaries_dir)
        if len(binaries) == 0:
            print(
                "You may want to compile the binary first with command:\n"
                "  python3 benchmark_vm.py --cmake <path_to_cmake_directory>"
            )

    benchmark_suite_files = get_benchmark_suite_files(
        args.programs_dir, args.inputs_dir, args.inputs_suite
    )

    print("Running benchmark on binaries:")
    for binary in binaries:
        print(bcolors.OKBLUE + "  - " + binary + bcolors.ENDC)

    print("Benchmark suite:")
    if len(benchmark_suite_files) > 0:
        for program, _ in benchmark_suite_files:
            print(bcolors.OKBLUE + "  - " + program + bcolors.ENDC)

        for binary in binaries:
            benchmark_one_vm(binary, benchmark_suite_files, args)
    else:
        print(bcolors.OKBLUE + "No benchmark programs found." + bcolors.ENDC)


def display_results(args):
    """
    Displays results from the csv's loaded
    from the "results" directory.
    """
    results = get_all_files_in_dir(args.results_dir, args.inputs_suite + CSV_EXTENSION)

    data = []
    for result in results:
        program_name = os.path.basename(result)[
            : -(1 + len(CSV_EXTENSION) + len(args.inputs_suite))
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
    print(
        f"\nResults for {bcolors.OKBLUE + args.inputs_suite + bcolors.ENDC} benchmark suite:"
    )
    print(
        " " * 4
        + "test name".ljust(25)
        + " "
        + "input".ljust(max_test_input_len + 1)
        + " "
        + "t_min [s]".rjust(10)
    )
    for program, results in processed_data:
        print()
        print(bcolors.HEADER + "--- " + program + ":" + bcolors.ENDC)
        for test_name, test_input, test_result in results:
            print(
                " " * 4
                + test_name.ljust(25)
                + " "
                + test_input.ljust(max_test_input_len + 1)
                + " "
                + time_or_dnf(test_result).rjust(10)
            )


def main():
    desc = """
When running the benchmark, place the binaries in the binaries directory.
You can specify the benchmark suite with the -i (--inputs_suite) option.
Default benchmark suite is "fast". Benchmark suites are defined inside the 
benchmark_inputs directory.

  python3 benchmark_vm.py -i <benchmark_suite_name>

There is a build tool to help you compile the binary. If you want to use it,
you need to specify the path to the CMake directory. The script will build
create the build directory where CMakeLists.txt is located, compile VM
and move the binary to the binaries directory. You can specify the CMake
target with the (-t) --target option.

  python3 benchmark_vm.py --cmake <path_to_cmake_directory> -t Vm_cg
  
If you want to benchmark only specific tests from suite, you can specify them in
BENCHMARK_ONLY list at the beginning of this file.

If you want to benchmark only specifit binaries you can specify them in "--only"
option in script argument.
"""

    parser = argparse.ArgumentParser(
        description=desc, formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # Benchmark options:
    parser.add_argument(
        "-i",
        "--inputs_suite",
        default=DEFAULT_BENCHMARK_SUITE,
        help="Name of the benchmark suite. Benchmark suites are defined inside "
        + "benchmark inputs directory.",
    )
    parser.add_argument(
        "-r",
        "--reps",
        default=5,
        type=int,
        help="Number of repetitions for each benchmark",
    )
    parser.add_argument(
        "--results_only",
        action="store_true",
        help="If specified, only displays results from results directory.",
    )
    parser.add_argument(
        "--only",
        help="If specified, only benchmark binary the given binary path",
        type=str,
    )

    # Compile options:
    parser.add_argument(
        "--cmake",
        help="If specified, builds the project with CMake from given directory.",
    )
    parser.add_argument(
        "-t",
        "--target",
        default=DEAFULT_DUCKLING_VM_TARGET,
        help="Name of the target to build with CMake.",
    )
    parser.add_argument(
        "-s",
        "--suffix",
        help="If specified, in the compiled binary name suffix will be appended.",
    )
    parser.add_argument(
        "-j",
        "--cpu_cores",
        default=DEFAULT_CPU_CORES,
        help="Number of CPU cores to use for compilation.",
        type=int,
    )

    # Directories options:
    parser.add_argument(
        "-b",
        "--binaries_dir",
        default=DUCKLING_VM_BINARIES_DIR,
        help="Name of the directory where VM binaries will be stored.",
    )
    parser.add_argument(
        "-p",
        "--programs_dir",
        default=BENCHMARK_PROGRAMS_DIR,
        help="Name of the directory where benchmark programs are stored.",
    )
    parser.add_argument(
        "--inputs_dir",
        default=BENCHMARK_INPUTS_DIR,
        help="Name of the directory where benchmark program's inputs are stored.",
    )
    parser.add_argument(
        "-o",
        "--results_dir",
        default=RESULTS_DIR,
        help="Name of the directory where benchmark results will be stored.",
    )

    args = parser.parse_args()

    if args.cmake:
        build_binary_and_save(args)
    elif args.results_only:
        display_results(args)
    else:
        run_benchmarks(args)
        display_results(args)


if __name__ == "__main__":
    main()
