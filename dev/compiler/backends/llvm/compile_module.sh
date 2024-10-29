# check that there is one arg:
if [ "$#" -ne 1 ]; then
	echo "Usage: compile_module.sh <module_name>"
	exit 1
fi

# compile llvm module to exe:

# get the module name:
module_name=$1

llvm-as $module_name.ll -o $module_name.bc
llc -filetype=obj $module_name.bc -o $module_name.o
gcc $module_name.o -o $module_name.exe
