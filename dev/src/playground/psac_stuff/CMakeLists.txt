
set(PSAC_FLAGS "-mcx16")

function (add_psac_exec name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} psac ${ARGN})
    target_compile_options(${name} PRIVATE ${PSAC_FLAGS})
endfunction()

add_psac_exec(psac_fib Base PstParser)
add_psac_exec(psac_array_sum Base PstParser)
