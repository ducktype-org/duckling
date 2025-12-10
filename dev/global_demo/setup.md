
# setup:
path+=~/.duck/bin/

# reset:
rm -r build_duck/*



# Examples

## Overloads
duckc compile_package global_demo/overloads/ -n overloads -a build_duck   
./build_duck/package_llvm.exe   --no-incremental

## Consts:
duckc get_hout ./global_demo/consts   

## DIA

duckc compile_package global_demo/dia -n dia -a build_duck --no-incremental   

## Class
duckc compile_package global_demo/class/ -n class -a build_duck --no-incremental --external-static-library ./global_demo/c_ffi/prints.c

./build_duck/package_llvm.exe  


## Floats

duckc compile_package global_demo/floats/ -n floats -a build_duck --no-incremental --external-static-library ./global_demo/c_ffi/prints.c

## Conversions

duckc compile_package global_demo/conversions/ -n conv -a build_duck --no-incremental --external-static-library ./global_demo/c_ffi/prints.c


## Tuple units
duckc get_hout ./global_demo/tuple_units


## Incremental