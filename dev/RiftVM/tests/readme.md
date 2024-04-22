# RiftVM tests

## Unit tests

To compile all RiftVm tests:
```
make build_vm_tests
```

To run one test from build directory:
```
ctest -R vm_micro_test
```

To run all RiftVm tests from the build directory and see the error output:
```
ctest -R vm_ --output-on-failure
```

Alternatively, you can run the tests from the `dev` directory:
```
./build/bin/vm_micro_test
```

## Performance tests

### Profiler installation

1. Intel VTune (2023.2.0+) is necessary to run profiler targets. Prior to
    installation, make sure that appropriate `linux-headers` are installed (they
    are necessary for the compilation of Intel profiling drivers during the install).

    On Debian-based systems it is probably best to use a provided APT repository:
    ```
    wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
        | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null

    echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] \
    https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list

    sudo apt update
    sudo apt install intel-oneapi-vtune
    ```
    For other systems (RHEL, OpenSUSE, other Linux, Windows, MacOS) visit [Intel VTune
    download section](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler-download.html).

2. To make VTune accessible from shell (either by `vtune` or `vtune-gui`), append
    this to your shell profile:
    ```
    source /opt/intel/oneapi/vtune/latest/env/vars.sh
    ```

3. Make sure that `perf_event_paranoid` is set to zero:
    ```
    sudo sysctl kernel.perf_event_paranoid=0
    ```
    **Note:** this setting can impose a risk of leaking sensitive data accessed
    by monitored processes as described
    [here](https://www.kernel.org/doc/html/latest/admin-guide/perf-security.html).
    If you accept this risk and want to make this setting persistent across
    reboots, you may create a sysctl rule:
    ```
    echo "kernel.perf_event_paranoid=0" | sudo tee -a /etc/10-perf.conf
    ```

### Running profiler targets

[performance/CMakeLists.txt](./performance/CMakeLists.txt) defines a set of
`profile_xyz` targets that execute RiftBC code inside a VM while collecting the
following results:
* performance snapshot (`ps`)
* hotspots (`hs`)
* microarchitecture utilization (`ua`)

**Note:** CMake will generate these targets only if it finds VTune in the path.

Results after every run are stored in
`dev/{cmake_build_folder}/results/profile_xyz-aa` directory where `aa` is a
shortcode of a given result. It is also possible to run all profiler targets at
once by issuing `make profile_all`.

**Note:** redoing a target overwrites above directories. If you need the
results in the future, make sure to copy them somewhere else or rename it using
`performance/scripts/rename_results.sh`.

