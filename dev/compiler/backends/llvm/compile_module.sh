#!/bin/bash

# Compiles single llvm module to exe

if [ "$#" -ne 1 ]; then
	echo "Usage: compile_module.sh <module_name>"
	exit 1
fi

# get the module name:
module_name=$1

llvm-as $module_name.ll -o $module_name.bc

# here we can also add some passes if needed

llc -filetype=obj $module_name.bc -o $module_name.o

# for now using gcc for linking:
gcc $module_name.o -o $module_name.exe
