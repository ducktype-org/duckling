set(PSAC_FLAGS "-mcx16")

function (add_psac_exec name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} psac ${ARGN})
    target_compile_options(${name} PRIVATE ${PSAC_FLAGS})
endfunction()

add_psac_exec(psac_array_sum array_sum/psac_array_sum.cpp Base Init)
add_example(query_array_sum array_sum/query_array_sum.cpp USES QueryFramework Base Init)

add_psac_exec(psac_fibonacci fibonacci/psac_fibonacci.cpp Base Init)
add_example(query_fibonacci fibonacci/query_fibonacci.cpp USES QueryFramework Base Init)

add_psac_exec(psac_dag dag/psac_dag.cpp Base Init)
add_example(query_dag dag/query_dag.cpp USES QueryFramework Base Init)
add_example(full_dag dag/full_dag.cpp USES Base Init)

add_subdirectory(compiler_test_corpus)
add_subdirectory(psac_compiler)
add_subdirectory(query_compiler)
