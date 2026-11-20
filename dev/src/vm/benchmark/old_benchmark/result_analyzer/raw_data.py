# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from analyzer import *

parameters = {
    COLLATZ: ("10^5", "10^7", "10^7"),
    FIB_ITER: ("10^7", "10^8", "10^9"),
    FIB_REC: ("30", "40", "44"),
}


def printResults(test_id):
    results = makeAllComputers()

    print(r"\begin{longtable}{|l|c|c|c|c|c|c|}")

    for k1, k2 in ((0, 1), (2, 3), (4, 5)):
        print(r"\hline")
        print(r" & \multicolumn{3}{|c|}{\textbf{Komputer ", end="")
        print(k1 + 1, end="")
        print(r"}} & \multicolumn{3}{|c|}{\textbf{Komputer ", end="")
        print(k2 + 1, end="")
        print(r"}} \\")

        p0 = parameters[test_id][0]
        p1 = parameters[test_id][1]
        p2 = parameters[test_id][2]
        print("\hline")
        print(
            f"Narzędzie & $n={p0}$ & $n={p1}$ & $n={p2}$& $n={p0}$ & $n={p1}$ & $n={p2}$",
            end="",
        )
        print(r"\\")

        for lang_id in range(LANG_COUNT):
            print("\hline")
            lang_name = HUMAN_NAME[LANG_NAME[lang_id]]

            for run_id in range(REPETITIONS):
                if run_id == 0:
                    print(f"\multirow{{5}}{{*}}{{{lang_name}}}", end="")

                for k in (k1, k2):
                    for case_id in range(TEST_CASES):
                        time = None
                        if (
                            not results[k]
                            .tests[test_id]
                            .langs[lang_id]
                            .cases[case_id]
                            .valid
                        ):
                            time = "---"
                        else:
                            time = (
                                results[k]
                                .tests[test_id]
                                .langs[lang_id]
                                .cases[case_id]
                                .runs[run_id]
                                .time
                            )
                            time = round(time, 2)
                            time = str(time) + "s"
                        print(f" & {time}", end="")
                print(r" \\")

        print()

    print("\hline")
    print(r"\end{longtable}")


if __name__ == "__main__":
    print(r"\section{Problem Collatza}")
    printResults(COLLATZ)

    print(r"\section{Iteracyjne obliczanie liczb Fibonacciego}")
    printResults(FIB_ITER)

    print(r"\section{Rekurencyjne obliczanie liczb Fibonacciego}")
    printResults(FIB_REC)
