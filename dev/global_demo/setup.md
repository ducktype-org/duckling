
# setup:
path+=~/.duck/bin/

# reset:
rm -r build_duck/*



# Examples

## Overloads
duckc compile_package global_demo/overloads/ -n overloads -a build_duck   
./build_duck/package_llvm.exe  

## consts:
duckc get_hout ./global_demo/consts   


## dia

duckc compile_package global_demo/dia -n dia -a build_duck --no-incremental   

