# Benchmark RiftVM versions

Folder structure:
```
├── old_benchmark
├── benchmark_riftvm.py
├── benchmark_inputs
├── benchmark_programs
├── results
└── rift_vm_bins
```

`old_benchmark` is a directory with the bash script used to benchmark different virtual machines: RiftVM, Java, Python NodeJS, C++. Written as a part of the 2023 RiftVM paper.

## Python script for comparing RiftVM versinons

To compare different RiftVM versions it is recommended to use `benchmark_riftvm.py` python script. It runs configured RiftVM binary file on a set of programs with specified inputs and saved the result. We distinguish between the following terms:
- RiftVM __binary file__
- __Program__ on which the performance of a virtual machine is measured
- __Inputs__ that are given to the program
- __Benchmark suite__ is a set of programs and inputs on which the execution time of RiftVm binary file is measured.

Directories:
- `benchmark_programs` - contains available programs
- `benchmark_inputs` - contains inputs given to the programs
- `rift_vm_bins` - contains RiftVM binary files
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
python3 benchmark_riftvm.py --help
```
By default, script benchmarks __all__ RiftVm's from the `rift_vm_bins` folder with __fast__ benchmark suite (defined by `DEFAULT_BENCHMARK_SUITE` global variable).
```bash
python3 benchmark_riftvm.py
```

To benchmark only __one__ binary you can:
```bash
python3 benchmark_riftvm.py --only rift_vm_bins/RiftVm_dev_1d9e5a2
```

To specify the benchmark suite:
```bash
python3 benchmark_riftvm.py -i long
```

You can configure the number of repetitions. As the final results the minimum is taken, but in the `.csv` file all results are saved.
```bash
python3 benchmark_riftvm.py -r 10
```

### Compile and move to riftvm_bins

Often the user wants to compile the current project and move the compiled binary to the `riftvm_bins` directory with the appropriate name. There is useful option for that use case:
```bash
python3 benchmark_riftvm.py --cmake ../..
``` 
It will compile project inside `build` directory and move the binary to the binaries folder. Binary file will be renamed to current branch and shortened commit hash, indicating current development version. To add some additional suffix you can:
```bash
python3 benchmark_riftvm.py --cmake ../.. --suffix computed_gotos
``` 