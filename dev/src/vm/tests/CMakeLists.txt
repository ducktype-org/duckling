include(CMakePrintHelpers)

# `ffi_test_lib` and `duck_use_ffi_test_lib`, used by the FFI suites below.
add_subdirectory(runtime/native/ffi)

# When VM runs code for a long time in TC mode, without the "tailcall" optimization
# enabled, it will run out of stack space.
function(duck_disable_tc_when_not_opt test_name test_set)
        if (test_set STREQUAL "vm_tc")
                if(NOT BUILD_TYPE_IS_OPTIMISED)
                        set_property(TEST ${test_set}_${test_name} PROPERTY DISABLED TRUE)
                endif()
        endif()
endfunction()

# @TODO: #3612 Remove this function.
function(duck_disable_when_jit test_name test_set)
        if(JIT_ENABLED)
                set_property(TEST ${test_set}_${test_name} PROPERTY DISABLED TRUE)
        endif()
endfunction()

function(duck_add_vm_tests suffix)
        set(libvm "LibVM_${suffix}")

        set(vmtester "VmTester_${suffix}")
        add_library(${vmtester}
                common/vm_tester_utils.cpp)
        target_include_directories(${vmtester}
                PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/common)
        target_link_libraries(${vmtester} PRIVATE
                ${libvm}
                Tester)

        set(test_set "vm_${suffix}")

        duck_add_test_custom(vm ${test_set}_primitives_test runtime/primitives/primitives_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_control_flow_test runtime/control_flow/control_flow_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_globals_test runtime/globals/globals_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_structures_test runtime/structures/structures_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_fixed_size_table_test runtime/arrays/fixed_size_table/fixed_size_table_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_dynamic_table_test runtime/arrays/dynamic_table/dynamic_table_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_memory_test runtime/memory/memory_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_runtime_errors_test runtime/errors/runtime_errors_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_api_test api/api_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt(api_test ${test_set})
        # api_test stays enabled in JIT builds: it spawns its processes with
        # enable_jit = false (#3612), so the JIT never interferes with stop/pause.
        duck_add_test_custom(vm ${test_set}_api_stress_test api/api_stress_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt(api_stress_test ${test_set})
        # Same for api_stress_test: it disables the JIT the same way (#3612).
        duck_add_test_custom(vm ${test_set}_functions_test runtime/functions/functions_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_correctness_test runtime/correctness/correctness_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt(correctness_test ${test_set})

        duck_add_test_custom(vm ${test_set}_debugger_test api/debug/debugger_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_when_jit(debugger_test ${test_set})

        duck_add_test_custom(vm ${test_set}_multithreaded_debugger_test
                api/debug/multithreaded_debugger_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt(multithreaded_debugger_test ${test_set})
        duck_disable_when_jit(multithreaded_debugger_test ${test_set})

        duck_add_test_custom(vm ${test_set}_debugger_class_test debugger/debugger_class_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_when_jit(debugger_class_test ${test_set})

        duck_add_test_custom(vm ${test_set}_debugger_infinite_test api/debug/debugger_infinite_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt("debugger_infinite_test" ${test_set})
        duck_disable_when_jit(debugger_infinite_test ${test_set})

        duck_add_test_custom(vm ${test_set}_inheritance_semantics_test runtime/inheritance/inheritance_semantics_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_code_injection_test api/injection/code_injection_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_run_function_test api/run_function/run_function_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_pointers_test runtime/pointers/pointers_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_variants_test runtime/variants/variants_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_extern_c_test runtime/native/extern_c/extern_c_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_ffi_test runtime/native/ffi/ffi_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_use_ffi_test_lib(${test_set}_ffi_test)
        duck_add_test_custom(vm ${test_set}_threads_test runtime/threads/threads_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt("threads_test" ${test_set})

        # @TODO: #761 re-enable on gcc >= 15
        if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15))
                duck_add_test_custom(vm ${test_set}_fast_mode_test fast_mode/fast_mode_test.cpp
                        USES ${libvm} Tester ${vmtester})
        endif()

        # Runtime errors that need several threads. A separate binary from runtime_errors_test, so
        # only these are skipped on TC when the build is not optimised.
        duck_add_test_custom(vm ${test_set}_thread_errors_test runtime/errors/threads/thread_errors_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt("thread_errors_test" ${test_set})


        duck_add_test_custom(vm ${test_set}_jit_test jit/jit_test.cpp
                USES ${libvm} Tester ${vmtester})

        if (NOT JIT_ENABLED)
                # This test does not directly invoke jit.
                # When jit IS enabled it helps to test it.
                set_property(TEST ${test_set}_jit_test PROPERTY DISABLED TRUE)
        endif()
endfunction()

duck_add_vm_tests("tc")
duck_add_vm_tests("sc")

# Everything below runs once, against the SC build only: these suites never execute a program
# (or only check the loader's output), so running them again on TC would test nothing new.

# utils/ — VM data structures, no program involved.
duck_add_test_custom(vm vm_stable_obj_id_name_map_test utils/stable_obj_id_name_map_test.cpp
        USES LibVM Tester)
duck_add_test_custom(vm vm_persistent_structures_test utils/persistent_structures_test.cpp
        USES LibVM Tester)

# debugger/ — the parts of the debugger tool that run no program: the CLI text helpers and the
# source mapper.
duck_add_test_custom(vm vm_debugger_cli_common_test debugger/debugger_cli_common_test.cpp
        USES LibVM_sc Tester)
duck_add_test_custom(vm vm_mapper_test debugger/mapper_test.cpp
        USES LibVM_sc Tester)

# bytecode/ — instruction set, program builders, the assembly parser and the serializer.
duck_add_test_custom(vm vm_instruction_naming_test bytecode/instruction_naming_test.cpp
        USES LibVM_sc Tester)
duck_add_test_custom(vm vm_program_builders_test bytecode/builders/program_builders_test.cpp
        USES LibVM)
duck_add_test_custom(vm vm_bc_parsing_test bytecode/assembly/bc_parsing_test.cpp USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_bc_serializer_test bytecode/serializer/bc_serializer_test.cpp USES LibVM_sc Tester VmTester_sc)

# loader/ — static verification, lowering (incl. the inheritance metadata), hierarchy checks and
# FFI declarations.
duck_add_test_custom(vm vm_bc_verification_test loader/verification/bc_verification_test.cpp USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_step_gil_placement_test loader/lowering/step_gil_placement_test.cpp USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_local_initialization_test loader/lowering/local_initialization_test.cpp USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_inheritance_loading_test loader/lowering/inheritance_loading_test.cpp
        USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_inheritance_hierarchy_correctness_test
        loader/inheritance/inheritance_hierarchy_correctness_test.cpp
        USES LibVM_sc Tester VmTester_sc)
duck_add_test_custom(vm vm_ffi_verification_test loader/ffi/ffi_verification_test.cpp
        USES LibVM_sc Tester VmTester_sc)
duck_use_ffi_test_lib(vm_ffi_verification_test)
