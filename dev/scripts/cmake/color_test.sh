#!/bin/bash

# `color_test.sh` is a script that modifies the output of `ctest` to add more colors. 
# It can be run with any arguments that are possible for the `ctest` command

esc=$(printf '\x1B')
make_green=$(printf "${esc}[32m&${esc}[0m")
make_red=$(printf "${esc}[91m&${esc}[0m")

ctest "$@" | sed -E \
"s@Passed@${make_green}@g;"\
"s@100% tests passed@${make_green}@g;"\
"s@\*\*\*Failed@${make_red}@g;"\
"s@^.*\(Failed\).*\$@${make_red}@g;"\
"s@ [123456789][0987654321]* tests failed@${make_red}@g"
