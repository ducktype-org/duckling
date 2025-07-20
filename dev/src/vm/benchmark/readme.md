# Duckling VM Benchmarks

# Benchmark VM versions

Folder structure:
```
├── old_benchmark
├── vm_benchmark.py
├── benchmark_inputs
├── benchmark_programs
├── results
└── vm_bins
```

`old_benchmark` is a directory with the bash script used to benchmark different virtual machines: VM, Java, Python NodeJS, C++. Written as a part of the 2023 VM paper.

- [out_files_structure](./old_benchmark/out_files_structure.md)
- [old_benchmark](./old_benchmark/readme.md)

## Python script for comparing VM versinons

To compare different VM versions it is recommended to use `vm_benchmark.py` python script. It runs configured VM binary file on a set of programs with specified inputs and saved the result. We distinguish between the following terms:
- VM __binary file__
- __Program__ on which the performance of a virtual machine is measured
- __Inputs__ that are given to the program
- __Benchmark suite__ is a set of programs and inputs on which the execution time of VM binary file is measured.

Directories:
- `benchmark_programs` - contains available programs
- `benchmark_inputs` - contains inputs given to the programs
- `vm_bins` - contains VM binary files
- `results` - place where `.csv` result files will be stored

To enable performance testing in several variants (ex. fast and long benchmark), we have introduced benchmark suites. They are defined by the set of __inputs__. Each input set has it's own directory inside `benchmark_inputs` directory:
```
├── benchmark_inputs
    ├── fast
    └── long
```
For example, such folder structure defines two benchmark suites: fast and long.

Every program from `benchmark_programs` is available to every benchmark suite. 

## Usage
### Benchmark
For usage info you can type:
```bash
python3 vm_benchmark.py
```
By default, script benchmarks __all__ VM's from the `vm_bins` folder with __fast__ benchmark suite (defined by `DEFAULT_BENCHMARK_SUITE` global variable).
```bash
python3 vm_benchmark.py run fast
```

To benchmark only __one__ binary you can:
```bash
python3 vm_benchmark.py run --only vm_bins/VM_dev_1d9e5a2
```

To specify the benchmark suite:
```bash
python3 vm_benchmark.py run long
```

You can configure the number of repetitions. As the final results the minimum is taken, but in the `.csv` file all results are saved.
```bash
python3 vm_benchmark.py run -r 10
```

### Compile and move to vm_bins

Often the user wants to compile the current project and move the compiled binary to the `vm_bins` directory with the appropriate name. There is a useful command for that use case:
```bash
python3 vm_benchmark.py compile ../..
``` 
It will compile project inside `build` directory and move the binary to the binaries folder. Executable VM file will be renamed to current branch and shortened commit hash, indicating current development version. To add some additional suffix you can:
```bash
python3 vm_benchmark.py compile ../.. --suffix computed_gotos
```

You can also set C++ compiler for project or pass custom CMake options, for detailed info check:
```bash
python3 vm_benchmark.py compile
```
