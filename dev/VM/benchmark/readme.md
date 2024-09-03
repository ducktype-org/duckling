\page vm-benchmark Duckling VM Benchmarks

# Benchmark VM versions

Folder structure:
```
├── old_benchmark
├── benchmark_vm.py
├── benchmark_inputs
├── benchmark_programs
├── results
└── duckling_vm_bins
```

`old_benchmark` is a directory with the bash script used to benchmark different virtual machines: VM, Java, Python NodeJS, C++. Written as a part of the 2023 VM paper.

\subpage vm-benchmark-old

## Python script for comparing VM versinons

To compare different VM versions it is recommended to use `benchmark_vm.py` python script. It runs configured VM binary file on a set of programs with specified inputs and saved the result. We distinguish between the following terms:
- VM __binary file__
- __Program__ on which the performance of a virtual machine is measured
- __Inputs__ that are given to the program
- __Benchmark suite__ is a set of programs and inputs on which the execution time of Vm binary file is measured.

Directories:
- `benchmark_programs` - contains available programs
- `benchmark_inputs` - contains inputs given to the programs
- `duckling_vm_bins` - contains VM binary files
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
python3 benchmark_vm.py --help
```
By default, script benchmarks __all__ Vm's from the `duckling_vm_bins` folder with __fast__ benchmark suite (defined by `DEFAULT_BENCHMARK_SUITE` global variable).
```bash
python3 benchmark_vm.py
```

To benchmark only __one__ binary you can:
```bash
python3 benchmark_vm.py --only duckling_vm_bins/Vm_dev_1d9e5a2
```

To specify the benchmark suite:
```bash
python3 benchmark_vm.py -i long
```

You can configure the number of repetitions. As the final results the minimum is taken, but in the `.csv` file all results are saved.
```bash
python3 benchmark_vm.py -r 10
```

### Compile and move to vm_bins

Often the user wants to compile the current project and move the compiled binary to the `vm_bins` directory with the appropriate name. There is useful option for that use case:
```bash
python3 benchmark_vm.py --cmake ../..
``` 
It will compile project inside `build` directory and move the binary to the binaries folder. Binary file will be renamed to current branch and shortened commit hash, indicating current development version. To add some additional suffix you can:
```bash
python3 benchmark_vm.py --cmake ../.. --suffix computed_gotos
``` 