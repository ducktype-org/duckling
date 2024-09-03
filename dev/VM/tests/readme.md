\page vm-tests VM Tests

\subpage vm-performance-tests 

## Unit tests

To compile all Vm tests:
```
make build_vm_tests
```

To run one test from build directory:
```
ctest -R vm_micro_test
```

To run all Vm tests from the build directory and see the error output:
```
ctest -R vm_ --output-on-failure
```

Alternatively, you can run the tests from the `dev` directory:
```
./build/bin/vm_micro_test
```

