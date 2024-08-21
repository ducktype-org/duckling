#!/bin/bash

# `color_ld_out.sh` is a script that modifies the error output of make to add highlighting to linker errors. 
# It can be run with any arguments that are possible for the `make` command. 
# It might not work well on other errors.

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

make "$@" 2>&1 >/dev/null | exec $parent_path/lib/color_ld_out.py
