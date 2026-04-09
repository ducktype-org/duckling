find_program(VTUNE vtune /opt/intel/oneapi/vtune/latest/bin64/)
if (NOT ${VTUNE} STREQUAL VTUNE-NOTFOUND)
    message(STATUS "VTune found, generating profiler targets")
    option(PROFILE "Add profiler targets" ON)
else ()
    message(STATUS "VTune not found, skipping profiler target generation")
endif ()

function(duck_add_perf_test test_pack test_name source USES)
    if (${USES} STREQUAL "USES")
        set(EXEC "prof_${test_name}")
        add_executable(${EXEC} ${CMAKE_CURRENT_LIST_DIR}/${source})
        target_link_libraries(${EXEC} Tester ${ARGN})
        target_compile_options(${EXEC} PRIVATE -g)

        set(target_name "profile_${test_name}")
        set(exec_path "${CMAKE_BINARY_DIR}/bin/perf/${EXEC}")
        set(results_path "${CMAKE_BINARY_DIR}/perf/results")

        add_custom_target("${target_name}"
                COMMAND mkdir -p ${results_path}
                COMMAND rm -rf "${results_path}/${target_name}*"
                COMMAND ${VTUNE} -collect performance-snapshot -r
                "${results_path}/${target_name}-ps" ${exec_path}
                COMMAND ${VTUNE} -collect hotspots -r
                "${results_path}/${target_name}-hs" ${exec_path}
                COMMAND ${VTUNE} -collect uarch-exploration -r
                "${results_path}/${target_name}-ua" ${exec_path}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        add_dependencies(${target_name} ${EXEC})
        add_dependencies("profile_all" ${target_name})

        set_target_properties(${EXEC} PROPERTIES EXCLUDE_FROM_ALL true)
        set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL true)
    else ()
        message(FATAL_ERROR "duck_add_perf_test lacks uses clause")
    endif ()
endfunction()

if (PROFILE)
    add_custom_target("profile_all")

    duck_add_perf_test(vm vm_fib_rec fib_rec.cpp
            USES LibVM Tester)
    duck_add_perf_test(vm vm_collatz collatz.cpp
            USES LibVM Tester)
    duck_add_perf_test(vm vm_ackermann ackermann.cpp
            USES LibVM Tester)
endif ()
