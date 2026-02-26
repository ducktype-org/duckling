process_to_binary(tests/memory/data.cpp)
duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)
target_include_directories(jit_memory_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(jit_memory_test PRIVATE "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}")

cmake_print_variables(jit_nm_out jit_text_out)

set_source_files_properties("${jit_nm_out}" PROPERTIES GENERATED TRUE)
set_source_files_properties("${jit_text_out}" PROPERTIES GENERATED TRUE)

target_sources(
    jit_memory_test 
    PRIVATE "${jit_nm_out}" "${jit_text_out}"
)
