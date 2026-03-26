set(PSAC_FLAGS "-mcx16")

function (add_psac_exec name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} psac ${ARGN})
    target_compile_options(${name} PRIVATE ${PSAC_FLAGS})
endfunction()

add_psac_exec(psac_array_sum Base PstParser)
add_example(query_array_sum query_array_sum.cpp USES QueryFramework Base)

add_psac_exec(psac_fibonacci Base PstParser)
add_example(query_fibonacci query_fibonacci.cpp USES QueryFramework Base)
