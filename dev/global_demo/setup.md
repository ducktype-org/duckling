
# setup:
path+=~/.duck/bin/

# reset:
rm -r build_duck/*



# Examples

## Overloads
duckc compile_package global_demo/overloads/ -n overloads -a build_duck --no-incremental

./build_duck/package_llvm.exe   

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

duckc compile_package ./global_demo/inc -a build_duck -n inc --no-incremental

graph:
duckc compile_package ./global_demo/inc -a build_duck -n inc  --print-graph --no-incremental

#{ Incremental:
 1. swap function order
 2. add comments, whitespaces
 3. change definition in other module
 4. change definition in other module, but is used in compile time
 5. add non-top level entities in other module
 6. modify default_n -- showcase false dependency
 7. add type to default_n and modify it again -- no more false dependencies now!
#}
