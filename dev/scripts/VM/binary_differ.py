#!/bin/python3

import re
import subprocess
import argparse
import tempfile

HELP_MESS = """
Smart differ of assembly changes between two versions of a binary. Example usage:
    python3 binary_differ.py --diff-view=column file1 file2
"""

DIFF_OPTIONS = "diff -i -E -b -B"
OBJDUMP_OPTIONS = "objdump -d -C --disassembler-options=intel --no-show-raw-insn --no-addresses"  # --visualize-jumps"


map1 = {}
map2 = {}


def parse_args():
    global arg, params, DIFF_OPTIONS, binaries
    parser = argparse.ArgumentParser(
        description=HELP_MESS, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "-V",
        "--diff-view",
        default="std",
        help="Diff output format. Options 'column' and 'std' are supported. Defaults to std diff",
    )
    parser.add_argument(
        "--skip-signature",
        help="Supress printing of function signature. Turned off by defualt.",
        action="store_true",
    )
    parser.add_argument("file1")
    parser.add_argument("file2")

    args = parser.parse_known_args()
    params = args[0]
    binaries = [params.file1, params.file2]

    if params.diff_view == "column":
        DIFF_OPTIONS += " -y --left-column --width=230"


def prepare_maps():
    global functions1, functions2, binaries, params
    f1 = subprocess.run(
        [OBJDUMP_OPTIONS + " " + binaries[0]], capture_output=True, shell=True
    )
    functions1 = f1.stdout.decode()
    functions1 = re.sub("0x[0-9a-f]*", "0x0", functions1)

    f2 = subprocess.run(
        [OBJDUMP_OPTIONS + " " + binaries[1]], capture_output=True, shell=True
    )
    functions2 = f2.stdout.decode()
    functions2 = re.sub("0x[0-9a-f]*", "0x0", functions2)

    if params.skip_signature:
        functions1 = re.sub("\(.*\)", "", functions1)
        functions2 = re.sub("\(.*\)", "", functions2)

    functions1 = functions1.split("\n\n")
    functions2 = functions2.split("\n\n")


def fill_maps(functions, map):
    global map1, map2
    for function in functions:
        function_name = re.findall("<.*>", function)
        if function_name.__len__() > 0:
            function_name_sigless = re.sub("\(.*", ">:", function_name[0])
            function_name_sigless = function_name_sigless[1:-2]
            # function_name_sigless_templateless = re.sub("<.*")
            # print(function_name_sigless)
            map.update({function_name_sigless: function})


def print_present_in_first(first, second, first_source):
    for func_name1, func_body1 in first.items():
        if second.get(func_name1) is None:
            print(first_source + func_body1)


def diff():
    file_tmp1 = tempfile.NamedTemporaryFile("w+")
    file_tmp2 = tempfile.NamedTemporaryFile("w+")
    for func_name2, func_body2 in map2.items():
        func_body1 = map1.get(func_name2)

        if func_body1 is None:
            continue

        file_tmp1.seek(0)
        file_tmp2.seek(0)
        file_tmp1.write(func_body1)
        file_tmp2.write(func_body2)
        file_tmp1.truncate()
        file_tmp2.truncate()

        res = subprocess.run(
            [DIFF_OPTIONS + " " + file_tmp1.name + " " + file_tmp2.name],
            capture_output=True,
            shell=True,
        )
        # print(res)
        if res.stdout is not None:
            decoded = res.stdout.decode()
            if decoded != "":
                if params.diff_view == "std":
                    print(func_name2 + "\n" + res.stdout.decode() + "\n\n", end="")
                else:
                    print(res.stdout.decode() + "\n\n", end="")


parse_args()

prepare_maps()

fill_maps(functions1, map1)
fill_maps(functions2, map2)

diff()

print_present_in_first(map1, map2, binaries[0])
print_present_in_first(map2, map1, binaries[1])
