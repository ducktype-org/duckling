import statistics
import sys

REPETITIONS = 5

LANG_COUNT = 10
TEST_CASES = 3

# Deprecated for „human names”
LANG_NAMES = [
    "C++",
    "C++ with gdb",
    "C++ with Valgrind",
    "Java with JIT",
    "Java without Jit",
    "NodeJS",
    "NodeJS without JIT",
    "Python",
    "RiftVM",
    "RiftVM with debug",
]

LANG_ID = {
    "cpp": 0,
    "gdb": 1,
    "valgrind": 2,
    "java": 3,
    "java_no_jit": 4,
    "node": 5,
    "node_no_jit": 6,
    "python": 7,
    "rift": 8,
    "rift_debug": 9,
}

LANG_NAME = {v: k for k, v in LANG_ID.items()}

assert len(LANG_NAME) == len(LANG_ID)

HUMAN_NAME = {
    "cpp": "C++",
    "gdb": "C++ GDB",
    "valgrind": "C++ Valgrind",
    "java": "JVM + JIT",
    "java_no_jit": "JVM",
    "node": "NodeJS + JIT",
    "node_no_jit": "NodeJS",
    "python": "Python3",
    "rift": "RiftVM",
    "rift_debug": "RiftVM + debug",
}

COLLATZ = 0
FIB_ITER = 1
FIB_REC = 2

SMALL = 0
MEDIUM = 1
LARGE = 2

COMPUTERS = 6

assert LANG_COUNT == len(LANG_NAMES)


def chunks(l: list, size: int):
    assert len(l) % size == 0
    return [l[i : i + size] for i in range(0, len(l), size)]


# Result of single run
class SingleRunResult:
    def __init__(self, lang_id, case_id, value):
        if value == "skip" or value == "timeout":
            self.time = None
        else:
            self.time = float(value)
        self.lang_id = lang_id
        self.case_id = case_id


# Result of single case
class TestCaseResult:
    def __init__(self, lang_id, case_id, values):
        assert len(values) == REPETITIONS

        self.lang_id = lang_id
        self.case_id = case_id

        self.runs = []
        for run in values:
            self.runs.append(SingleRunResult(lang_id, case_id, run))

        self.avg = self._getAvg()

        self.valid = True
        for e in self.runs:
            if e.time is None:
                self.valid = False

    def _getAvg(self):
        for e in self.runs:
            if e.time is None:
                return None
        times = [e.time for e in self.runs]
        return statistics.fmean(times)

    def getAvg(self):
        return self.avg


# Result of entire lang
class LangResult:
    def __init__(self, lang_id, values):
        assert len(values) == REPETITIONS * TEST_CASES

        self.lang_id = lang_id

        self.cases = []
        cases = chunks(values, REPETITIONS)

        for i, case in enumerate(cases):
            self.cases.append(TestCaseResult(lang_id, i, case))

    def getAvgOfCase(self, case_id):
        return self.cases[case_id].getAvg()


# Result of all
class TestResult:
    def __init__(self, values, file):
        assert len(values) == REPETITIONS * TEST_CASES * LANG_COUNT

        self.file = file
        self.langs = []
        langs = chunks(values, REPETITIONS * TEST_CASES)

        for i, lang in enumerate(langs):
            self.langs.append(LangResult(i, lang))

    def _generateRaw(self, raw):
        for case in range(TEST_CASES):
            print(f"\t\\testinput{'I'*(case + 1)}  ", end="")
            for lang in raw:
                avg = self.langs[LANG_ID[lang]].getAvgOfCase(case)
                val = "Error"
                if avg is not None:
                    val = f"${avg:.3f}$s"
                else:
                    val = " --- "
                print(f"& {val} ", end="")

            print(r"\\")

    def generateBenchmarkTable(self):
        first_raw = ["rift", "rift_debug", "java_no_jit", "java", "python"]
        second_raw = ["cpp", "gdb", "valgrind", "node_no_jit", "node"]

        print(f"% From file: {self.file}")
        print(r"\benchmarktable{")
        self._generateRaw(first_raw)
        print(r"}{")
        self._generateRaw(second_raw)
        print(r"}{Komputer ???}")


class AllTestResult:
    def __init__(self, computer_id, collatz, fib_iter, fib_rec):
        self.computer_id = computer_id
        self.tests = [collatz, fib_iter, fib_rec]


def makeDataFromFile(file: str):
    data = []
    with open(file) as result:
        raw_lines = result.readlines()
        for line in raw_lines:
            line = line.strip()
            if line != "":
                data.append(line)
    assert len(data) == REPETITIONS * TEST_CASES * LANG_COUNT
    return TestResult(data, file)


def makeAllDataFromFile(folder: str, comp_id: int):
    return AllTestResult(
        comp_id,
        makeDataFromFile(folder + "/collatz.out"),
        makeDataFromFile(folder + "/fib_iter.out"),
        makeDataFromFile(folder + "/fib_rec.out"),
    )


def makeAllComputers():
    out = []
    out.append(makeAllDataFromFile("final_results/kacper_09.06_1230", 1))
    out.append(makeAllDataFromFile("final_results/andrzej_9.06_2207", 2))
    out.append(makeAllDataFromFile("final_results/kacper2_10.06_0130", 3))
    out.append(makeAllDataFromFile("final_results/jakub_9.06.2023", 4))
    out.append(makeAllDataFromFile("final_results/kacperL_11.06_1230", 5))
    out.append(makeAllDataFromFile("final_results/hubert_12.06_0030", 6))

    assert len(out) == COMPUTERS
    return out


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 analyzer.py result.out")
        exit()
    result_file = sys.argv[1]

    test_result = makeDataFromFile(result_file)

    test_result.generateBenchmarkTable()
