
# setup:
path+=~/.duck/bin/

# compile:
duckc compile_package global_demo/overloads/ -n overloads -a build_duck         

# run:
./build_duck/package_llvm.exe  

# reset:
rm -r build_duck/*


# hout:
duckc get_hout ./global_demo/consts   
