#!/usr/bin/env python3
"""Collect coverage from a clang source-based-coverage test run, in parallel.

Each test executable is paired with the profile its own test produced. That
pairing is the whole point: llvm-cov resolving one object against a profile
merged from every test takes ~18 s, against that test's own profile ~13 ms,
because a merged profile holds many same-name-different-hash records and every
lookup pays for them.

A source file only appears in llvm-cov's output if it is part of some object
passed on the command line, so files no test executable links would silently
vanish from the report and inflate the percentage. To match what gcov's
--process-gcno gave us, --baseline-dir sweeps every object file in the build
tree and contributes their lines at zero. Baseline counts are always forced to
zero: the real counts come from the test/profile pairs, and the profile a
baseline chunk is resolved against is an arbitrary one, so its counters mean
nothing here.

Work is handed out through a shared queue, largest chunk first. The test
executables span three orders of magnitude in size, so splitting them into
fixed per-worker slices leaves most cores idle behind the slowest slice
(measured: 3.9 s with fixed slices, 2.4 s with a queue).

Line data is always collected; function data costs ~85% more (2.3 s -> 4.2 s
here) and nothing reads it, so it is opt-in via --functions. Branch and
expansion data are never collected - the gcov report this replaces has none.
"""
import argparse
import multiprocessing as mp
import os
import subprocess
import sys
import tempfile
import traceback

# Objects per baseline llvm-cov invocation. Small enough that the sweep spreads
# over the worker pool, large enough that process startup does not dominate.
BASELINE_CHUNK_SIZE = 150


class Task:
    """One llvm-cov invocation: some objects resolved against some profiles."""

    __slots__ = ("objects", "profiles", "is_baseline", "size")

    def __init__(self, objects, profiles, is_baseline):
        self.objects = objects
        self.profiles = profiles
        self.is_baseline = is_baseline
        self.size = sum(os.path.getsize(obj) for obj in objects)


def export(task, profdata, export_cmd, profdata_bin, lines_by_source, functions_by_source,
           want_functions):
    """Run one llvm-cov export, merging its records into the accumulators."""
    merge = [profdata_bin, "merge", "-sparse", "-o", profdata, *task.profiles]
    if subprocess.call(merge, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        return "llvm-profdata merge failed: " + ", ".join(task.profiles)

    command = [*export_cmd, "-instr-profile=" + profdata]
    for obj in task.objects:
        command += ["-object", obj]

    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    lines = functions = None
    for record in process.stdout:
        if record.startswith(b"DA:"):
            if lines is None:
                continue
            number, _, rest = record[3:].partition(b",")
            number = int(number)
            count = 0 if task.is_baseline else int(rest.split(b",", 1)[0])
            if count > lines.get(number, -1):
                lines[number] = count
        elif record.startswith(b"SF:"):
            source = record[3:].rstrip()
            lines = lines_by_source.setdefault(source, {})
            if want_functions:
                functions = functions_by_source.setdefault(source, {})
        elif record.startswith(b"FNDA:"):
            if functions is None:
                continue
            count, _, name = record[5:].rstrip().partition(b",")
            count = 0 if task.is_baseline else int(count)
            entry = functions.get(name)
            if entry is None:
                functions[name] = [None, count]
            elif count > entry[1]:
                entry[1] = count
        elif record.startswith(b"FN:"):
            if functions is None:
                continue
            number, _, name = record[3:].rstrip().partition(b",")
            entry = functions.setdefault(name, [None, 0])
            entry[0] = int(number)
        elif record.startswith(b"end_of_record"):
            lines = functions = None
    process.stdout.close()
    if process.wait() != 0:
        return "llvm-cov export failed: " + ", ".join(task.objects)
    return None


def worker(results, tasks, profdata_bin, cov_bin, ignore_regex, want_functions):
    """Export every task the queue hands out, merging as we go.

    Whatever happens, a result is put on the queue exactly once: a worker that
    died without reporting would leave the parent blocked on `results.get()`
    for good.
    """
    lines_by_source, functions_by_source, failures = {}, {}, []
    try:
        export_cmd = [cov_bin, "export", "-format=lcov",
                      "--skip-branches", "--skip-expansions"]
        if not want_functions:
            export_cmd.append("--skip-functions")
        if ignore_regex:
            export_cmd.append("-ignore-filename-regex=" + ignore_regex)

        handle, profdata = tempfile.mkstemp(suffix=".profdata")
        os.close(handle)
        try:
            for task in iter(tasks.get, None):
                failure = export(task, profdata, export_cmd, profdata_bin,
                                 lines_by_source, functions_by_source, want_functions)
                if failure:
                    failures.append(failure)
        finally:
            os.unlink(profdata)
    except BaseException:
        failures.append("coverage worker crashed:\n" + traceback.format_exc())
    finally:
        results.put((lines_by_source, functions_by_source, failures))


def combine_lines(into, chunk):
    """Union the per-line hit counts: a line is covered if any test covered it."""
    for source, lines in chunk.items():
        known = into.get(source)
        if known is None:
            into[source] = lines
            continue
        for number, count in lines.items():
            if count > known.get(number, -1):
                known[number] = count


def combine_functions(into, chunk):
    for source, functions in chunk.items():
        known = into.get(source)
        if known is None:
            into[source] = functions
            continue
        for name, (number, count) in functions.items():
            entry = known.get(name)
            if entry is None:
                known[name] = [number, count]
                continue
            if number is not None:
                entry[0] = number
            if count > entry[1]:
                entry[1] = count


def collect(tasks_to_run, jobs, profdata_bin, cov_bin, ignore_regex, want_functions):
    results, tasks = mp.Queue(), mp.Queue()
    for task in tasks_to_run:
        tasks.put(task)

    workers = []
    for _ in range(min(jobs, len(tasks_to_run))):
        tasks.put(None)
        process = mp.Process(target=worker, args=(results, tasks, profdata_bin, cov_bin,
                                                  ignore_regex, want_functions))
        process.start()
        workers.append(process)

    all_lines, all_functions, failures = {}, {}, []
    for _ in workers:
        chunk_lines, chunk_functions, chunk_failures = results.get()
        combine_lines(all_lines, chunk_lines)
        combine_functions(all_functions, chunk_functions)
        failures.extend(chunk_failures)
    for process in workers:
        process.join()
    return all_lines, all_functions, failures


def write_tracefile(path, lines_by_source, functions_by_source):
    """Write the raw tracefile.

    Raw, because llvm-cov reports lines that hold no code: clang emits a skipped
    region for preprocessor-disabled code and the region's bounds reach the blank
    lines and comments around the directive, and it can name a line past the end of
    the file (llvm/llvm-project#120071). `lcov --filter blank,brace,range` drops all
    three, and does it against the source rather than by guessing - see the coverage
    targets in ConfigureTests.cmake.
    """
    total_found = total_hit = 0
    with open(path, "w") as tracefile:
        for source in sorted(lines_by_source):
            lines = lines_by_source[source]
            hit = sum(1 for count in lines.values() if count)
            tracefile.write("TN:\nSF:%s\n" % source.decode())

            functions = functions_by_source.get(source)
            if functions:
                known = {name: entry for name, entry in functions.items()
                         if entry[0] is not None}
                for name, (number, _) in sorted(known.items()):
                    tracefile.write("FN:%d,%s\n" % (number, name.decode()))
                for name, (_, count) in sorted(known.items()):
                    tracefile.write("FNDA:%d,%s\n" % (count, name.decode()))
                tracefile.write("FNF:%d\nFNH:%d\n"
                                % (len(known), sum(1 for _, c in known.values() if c)))

            for number in sorted(lines):
                tracefile.write("DA:%d,%d\n" % (number, lines[number]))
            tracefile.write("LF:%d\nLH:%d\nend_of_record\n" % (len(lines), hit))
            total_found += len(lines)
            total_hit += hit
    return total_found, total_hit


def executables_in(directory):
    return [os.path.join(directory, name) for name in sorted(os.listdir(directory))
            if os.path.isfile(os.path.join(directory, name))
            and os.access(os.path.join(directory, name), os.X_OK)]


def profiles_in(directory):
    return [os.path.join(directory, name) for name in sorted(os.listdir(directory))
            if name.endswith(".profraw")]


def find_test_tasks(binary_dir, profile_dir):
    """Pair every test executable with the profiles its own test run produced.
    

    Tests write `<test>.<pid>.profraw`, so a test that forks instrumented
    children contributes several files rather than having the children
    overwrite the parent.
    """
    profiles = {}
    for path in profiles_in(profile_dir):
        # `<test>.<pid>.profraw` -> `<test>`
        test_name = os.path.basename(path).rsplit(".", 2)[0]
        profiles.setdefault(test_name, []).append(path)

    tasks, unprofiled = [], []
    for binary in executables_in(binary_dir):
        name = os.path.basename(binary)
        if name in profiles:
            tasks.append(Task([binary], profiles[name], is_baseline=False))
        else:
            unprofiled.append(name)
    return tasks, unprofiled


def find_pooled_task(binary_dir, profile_dir):
    """One task over every binary, resolved against one shared profile pool.

    This is how the integration tests are collected: they run duckc and the VM
    without LLVM_PROFILE_FILE, so every process merges into the `%8m` pool
    compiled into the binaries and nothing identifies which run wrote what.
    """
    binaries = executables_in(binary_dir)
    profiles = profiles_in(profile_dir)
    if not binaries or not profiles:
        return []
    return [Task(binaries, profiles, is_baseline=False)]


def find_baseline_tasks(baseline_dir, profiles):
    """Sweep the build tree's object files so never-linked sources report as 0%.

    The profiles these resolve against are arbitrary - only the set of lines
    matters, and `export` forces baseline counts to zero.
    """
    objects = []
    for root, _, names in os.walk(baseline_dir):
        objects += [os.path.join(root, name) for name in names if name.endswith(".o")]
    objects.sort()
    return [Task(objects[at:at + BASELINE_CHUNK_SIZE], profiles, is_baseline=True)
            for at in range(0, len(objects), BASELINE_CHUNK_SIZE)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary-dir", required=True,
                        help="directory holding the instrumented test executables")
    parser.add_argument("--profile-dir", required=True,
                        help="directory holding <test-name>.<pid>.profraw")
    parser.add_argument("--pooled", action="store_true",
                        help="treat the profile directory as one shared pool covering "
                             "every binary, instead of pairing profiles with tests by "
                             "name (this is how the integration tests are collected)")
    parser.add_argument("--baseline-dir",
                        help="build tree to sweep for object files, so that sources no "
                             "test executable links still appear in the report at 0%%")
    parser.add_argument("--output", required=True, help="lcov tracefile to write")
    parser.add_argument("--jobs", type=int, default=len(os.sched_getaffinity(0)))
    parser.add_argument("--ignore-filename-regex", default="")
    parser.add_argument("--llvm-profdata", default="llvm-profdata")
    parser.add_argument("--llvm-cov", default="llvm-cov")
    parser.add_argument("--functions", action="store_true",
                        help="also collect per-function coverage (~85%% slower)")
    args = parser.parse_args()

    if args.pooled:
        tasks, unprofiled = find_pooled_task(args.binary_dir, args.profile_dir), []
        if not tasks:
            sys.exit("no profile in %s, or no binary in %s - were the integration "
                     "tests run against this build directory?"
                     % (args.profile_dir, args.binary_dir))
    else:
        tasks, unprofiled = find_test_tasks(args.binary_dir, args.profile_dir)
        if not tasks:
            sys.exit("no binary in %s has a profile in %s - did the tests run with "
                     "LLVM_PROFILE_FILE set?" % (args.binary_dir, args.profile_dir))
    test_count = len(tasks)

    if args.baseline_dir:
        tasks += find_baseline_tasks(args.baseline_dir, tasks[0].profiles)

    # Longest first, so no worker is still on an 87 MB binary when the queue drains.
    tasks.sort(key=lambda task: -task.size)

    lines, functions, failures = collect(tasks, args.jobs, args.llvm_profdata, args.llvm_cov,
                                         args.ignore_filename_regex, args.functions)

    # Report nothing rather than something wrong: a partial tracefile left on
    # disk would be picked up by genhtml and read as a real coverage drop.
    if failures:
        sys.exit("\n".join(failures))

    found, hit = write_tracefile(args.output, lines, functions)

    # Expected for tests ctest reports as Disabled, and for a filtered test run.
    if unprofiled:
        print("no profile from %d of %d executables (not run?): %s"
              % (len(unprofiled), len(unprofiled) + test_count, ", ".join(sorted(unprofiled))),
              file=sys.stderr)
    # Raw numbers: `lcov --filter` still has to strip the non-code lines llvm-cov
    # reports, so the percentage in the final report is a little higher than this.
    scope = "pooled profiles" if args.pooled else "%d tests" % test_count
    print("%s, %d files, %d of %d lines covered (%.2f%%, before filtering)"
          % (scope, len(lines), hit, found, hit / found * 100 if found else 0))


if __name__ == "__main__":
    main()
