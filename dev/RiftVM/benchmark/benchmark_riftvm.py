import argparse
import subprocess
import os
import random
from time import sleep
import time
import datetime 


RIFT_VM_TARGET = "RiftVm"
RIFT_VM_BINARY_PATH = "./bin/" + RIFT_VM_TARGET
RIFT_BC_EXT = ".rbc"
BUILD_DIR_NAME = "build"
CPU_CORES = 12
BENCHMARK_ONLY = []
# BENCHMARK_ONLY = ["ackermann.rbc"]


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    GREEN = '\033[32m'
    YELLOW = '\33[33m'
    RED = '\033[31m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def run_command_with_time(command: list, stdin: str):
    env_vars = {"TIMEFORMAT": "%R", "LC_NUMERIC": "en_US.UTF-8"}
    prefix = ["time", "-f", "%U"]
    full_command = prefix + command
    return subprocess.run(full_command, input=stdin, 
                          capture_output=True, text=True, env=env_vars)


def cmake_build_binary(cmake_path):
    """ Builds the project with CMake. 
        Path to cmake should be relative to the current directory.
        Returns absolute path to the compiled binary. """
    current_directory = os.getcwd()
    os.chdir(cmake_path)
    subprocess.run(["cmake", "-S", ".", "-B", BUILD_DIR_NAME, "-DCMAKE_BUILD_TYPE=Release"])
    result = subprocess.run(["cmake", "--build", BUILD_DIR_NAME, "--target", RIFT_VM_TARGET, "-j", str(CPU_CORES)])
    result.check_returncode()

    riftvm_bin_abs_path = os.path.abspath(os.path.join(BUILD_DIR_NAME, RIFT_VM_BINARY_PATH))
    os.chdir(current_directory)
    return riftvm_bin_abs_path


def run_riftvm(rift_vm_bin_path, input_file, stdin):
    # to build path add RIFT_VM_BINARY
    # send input to file
    result = run_command_with_time([rift_vm_bin_path, "-f", input_file], stdin)

    stderr_lines = result.stderr.split("\n")
    measured_time = float(stderr_lines[-2])

    if (len (stderr_lines) > 2):
        print(bcolors.RED + "Got abnormal stderr output.\n" +
              "Make sure binary does not print anything to stderr.")
        print("Return code: " + str(result.returncode))
        print("stderr: " + result.stderr + bcolors.ENDC)
        measured_time = -1

    return [result.stdout, measured_time]


def time_or_dnf(measured_time):
    return str(measured_time) if measured_time > 0 else "DNF"


def get_benchmark_times(rift_vm_bin_path, input_file, stdin, repeats=5):
    times = []
    benchmark_filename = os.path.basename(input_file)
    print(benchmark_filename[:29].ljust(30) + stdin.replace("\n", " ")[:29].ljust(30))

    for i in range(repeats):
        [_, time] = run_riftvm(rift_vm_bin_path, input_file, stdin)
        times.append(time)
        
        print(" "*4 + time_or_dnf(time))
        
        sleep(random.uniform(0, 0.5))
    
    return times


def get_git_head_info():
    result = subprocess.run(["git", "branch", "--show-current"], capture_output=True, text=True)
    branch_name = result.stdout.strip()
    result = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True)
    commit_hash = result.stdout.strip()

    return [branch_name, commit_hash]


def get_stdin_list(program_src):
    stdin_file = program_src.replace(".rbc", ".in")
    data = ""
    with open(stdin_file, "r") as f:
        data = f.read()
    
    return data.split("\n\n")


def create_dir_if_not_exists(dir_name):
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)


def save_benchmark_results(raport_path, results):
    with open(raport_path, "w") as f:
        for code, stdin, times in results:
            program_filename = os.path.basename(code)
            f.write((program_filename + ",").ljust(20))
            f.write((str(stdin).replace("\n"," ") + ",").ljust(15))
            for exec_time in times:
                f.write((str(exec_time) + ",").ljust(10))
            f.write("\n")
    
    print(bcolors.GREEN + f"Results saved to: {raport_path}" + bcolors.ENDC)


def benchmark_riftvm(rift_vm_bin_path, benchmark_codes, args):
    binary_name = os.path.basename(rift_vm_bin_path)
    raport_path = os.path.join(args.results_dir, f"{binary_name}.csv")
    results = list()

    print(f"\nRunning benchmark for:")
    print(bcolors.HEADER + binary_name + bcolors.ENDC)
    
    start = time.time()

    for code in benchmark_codes:
        for stdin in get_stdin_list(code):
            print("-" * 70)
            stdin_times = get_benchmark_times(rift_vm_bin_path, code, 
                                            stdin, repeats=args.reps)
            results.append((code, stdin, stdin_times))

    end = time.time()
    print(f"Elapsed time: " + str(datetime.timedelta(seconds=int(end - start))))

    save_benchmark_results(raport_path, results)
    
    return results


def get_all_files_in_dir(dir_name, extension = None):
    """ Returns list of paths to all files in a given directory
        with given extension. """
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
        bin_path = cmake_build_binary(args.cmake_dir)

    except subprocess.CalledProcessError as err:
        # If return code is non-zero, raise a CalledProcessError and print this message
        print(bcolors.RED + "Compilation error..." + bcolors.ENDC)
        return


    [branch, commit] = get_git_head_info()
    new_bin_path = os.path.join(args.binaries_dir, f"riftvm_{branch}_{commit}")

    if args.suffix:
        new_bin_path = new_bin_path + "_" + args.suffix
    
    while os.path.exists(new_bin_path):
        new_bin_path += datetime.datetime.now().strftime("_%y%m%d%H%M%S")
    
    subprocess.run(["mv", bin_path, new_bin_path])

    print(bcolors.GREEN + "Compiled binary to " + new_bin_path + bcolors.ENDC)


def run_benchmarks(args):
    """
        If args.only_benchmark is specified, only benchmarks the given binary.
        Otherwise, benchmarks all binaries in the binaries directory.
    """

    if args.only:
        binaries = [args.only]
    else:
        binaries = get_all_files_in_dir(args.binaries_dir)


    create_dir_if_not_exists(args.results_dir)
    
    print("Found binaries:")
    for binary in binaries:
        print(bcolors.OKBLUE + "  - " + binary + bcolors.ENDC)
    
    benchmark_codes = get_all_files_in_dir(args.codes_dir, RIFT_BC_EXT)
    if BENCHMARK_ONLY and len(BENCHMARK_ONLY) > 0:
        benchmark_codes = [os.path.join(args.codes_dir, code) for code in BENCHMARK_ONLY]

    print("Benchmarking codes:")
    for code in benchmark_codes:
        print(bcolors.OKBLUE + "  - " + code + bcolors.ENDC)

    for binary in binaries:
        benchmark_riftvm(binary, benchmark_codes, args)


def display_results(args):
    """
        Displays results from the csv's loaded
        from the "results" directory.
    """
    results = get_all_files_in_dir(args.results_dir, ".csv")
    
    data = []
    for result in results:
        program_name = os.path.basename(result)[:-4]
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
    processed_data = []
    for program_name, file_data in data:
        processed_file = list()
        for test_name, test_input, test_runs in file_data:
            test_runs = [float(run) for run in test_runs]
            processed_file.append((test_name, test_input, min(test_runs)))

        processed_data.append((program_name, processed_file))
    
    # display results for every program
    print(" "*4 + "test name".ljust(20) + " " + "input".ljust(30) + " " + "t_min [s]".ljust(30))
    for program, results in processed_data:
        print()
        print(bcolors.HEADER +"--- " + program + ":" + bcolors.ENDC)
        for test_name, test_input, test_result in results:
            print(" "*4 + test_name.ljust(20) + " " + test_input.ljust(30) + " " +
                time_or_dnf(test_result).ljust(30))
            


def main():
    desc = """
When running the benchmark, place the binaries in the binaries directory.

There is a build tool to help you compile the binary. If you want to use it,
you need to specify the path to the CMake directory. The script will build
create the build directory where CMakeLists.txt is located, compile RiftVM
and move the binary to the binaries directory.

  python3 benchmark_riftvm.py --cmake <path_to_cmake_directory>

If you want to benchmark only specific tests from suite, you can specify them in
BENCHMARK_ONLY list at the beginning of this file.

If you want to benchmark only specifit binaries you can specify them in "--only"
option in script argument.
"""

    parser = argparse.ArgumentParser(description=desc, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cmake_dir", help="If specified, builds the project with CMake from given directory")
    parser.add_argument("-b", "--binaries_dir", default="rift_vm_bins",
                         help="Name of the directory where RiftVM binaries will be stored")
    parser.add_argument("-c", "--codes_dir", default="rift_vm_codes", 
                        help="Name of the directory where codes to benchmark will be stored")
    parser.add_argument("-o", "--results_dir", default="results",
                        help="Name of the directory where benchmark results will be stored")
    parser.add_argument("-r", "--reps", default=5, type=int,
                        help="Number of repetitions for each benchmark")
    parser.add_argument("-s", "--suffix",
                        help="If specified, in the compiled binary name suffix will be appended.")
    parser.add_argument("--results_only", action="store_true",
                        help="If specified, only displays results from results directory.")
    parser.add_argument("--only",
                        help="If specified, only benchmark binary the given binary path", type=str)
    
    args = parser.parse_args()

    if args.cmake_dir:
        build_binary_and_save(args)
    elif args.results_only:
        display_results(args)
    else:
        run_benchmarks(args)
        display_results(args)



if __name__ == "__main__":
    main()
