# Rift VM Old Benchmark

- \subpage vm-old-out-files-structure
- \subpage vm-old-andrzej-raport

# Machine Versions:
JavaScript: 20.2.0 from https://nodejs.org/en
Python: 3.11.3 from https://www.python.org/downloads/release/python-3113/
Java 20.0.1 from https://jdk.java.net/20/

python: ./configure --enable-optimizations --with-lto and then

# Execution:
Remember to use the correct path to the latest version; JDK and nodejs come pre-compiled, python installed as python3.11 on my machine.

Set the computer to the highest performance settings reasonably possible. Ensure no unnecessary programs are running in the background and run from a clean terminal.

## JavaScript:
Two execution versions
1. With JIT: `time node {file}.js < input{n}.in > dump.out`
2. Without JIT: `time node --jitless {file}.js < input{n}.in > dump.out`
## Python:
`time python3 {file}.py < input{n}.in > dump.out`
## Java:
`javac {file}.java`
Two execution versions
1. With JIT: `time java {file} < input{n}.in > dump.out`
2. Without JIT: `time java -Xint {file} < input{n}.in > dump.out`

## RiftVM:
Two versions:
1. RiftVM + debug:
In src/services/executor_f8/op_case_config.hpp:
```
// #define USE_COMPUTED_GOTO
// #define USE_FLAT_FRAME
```
`time ./RiftVM -f {file}.rbc < input{n}.in > dump.out`
2. RiftVM:
In src/services/executor_f8/op_case_config.hpp:
```
#define USE_COMPUTED_GOTO
// #define USE_FLAT_FRAME
```
`time ./RiftVM -f {file}.rbc < input{n}.in > dump.out`

# Reporting
In Chapter 7 of the thesis, you must enter your processor, the amount and type of RAM (DDR3/DDR4, etc.), and your operating system. Next, fill in the appropriate tables with the speed in seconds, rounded to 2 decimal places. We use the "real" time.

It would be good to find someone with an ARM processor on their computer, as that could be interesting.
