duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)
process_to_binary(tests/memory/data.cpp jit_memory_test)

#add_files(copy-and-patch.cpp)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
