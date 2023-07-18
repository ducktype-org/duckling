# Set correct values here:

. local_config.sh

# Don't change it:

run_python="$python_bin"

run_java="$java_bin"
run_java_no_jit="$java_bin -Xint"

run_node="$node_bin"
run_node_no_jit="$node_bin --jitless --no-expose-wasm"

run_rbc="$rift_vm_bin -f"
run_rbc_debug="$rift_vm_bin_debug -f"

run_cpp=""
run_cpp_valgrind="valgrind -q "
run_cpp_gdb="gdb -return-child-result -batch-silent -nx -ex run"
