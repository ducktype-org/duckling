## Make scripts

These scripts should be used in a directory where `make` is enabled. For ease of use their copies are located in subdirectory script/ of the build directory. Any use of make should copy the changes from their original location to the build directory.

1. `color_test.sh` is a script that modifies the output of `ctest` to add more colors. It can be run with any arguments that are possible for the `ctest` command
1. `color_ld_out.sh` is a script that modifies the error output of make to add highlighting to linker errors. It can be run with any arguments that are possible for the `make` command. It might not work well on other errors.
