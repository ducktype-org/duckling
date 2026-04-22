include(CMakePrintHelpers)

# When VM runs code for a long time in TC mode, without the "tailcall" optimization
# enabled, it will run out of stack space.
function(duck_disable_tc_when_not_opt test_name test_set)
        if (test_set STREQUAL "vm_tc")
                if(NOT BUILD_TYPE_IS_OPTIMISED)
                        set_property(TEST ${test_set}_${test_name} PROPERTY DISABLED TRUE)
                endif()
        endif()
endfunction()

function(duck_add_vm_tests suffix)
        set(libvm "LibVM_${suffix}")

        set(vmtester "VmTester_${suffix}")
        add_library(${vmtester}
                utils/vm_tester_utils.cpp)
        target_include_directories(${vmtester}
                PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/utils)
        target_link_libraries(${vmtester} PRIVATE
                ${libvm}
                Tester)

        set(test_set "vm_${suffix}")

        duck_add_test_custom(vm ${test_set}_micro_test basic/working_vm.cpp
                USES ${libvm} Tester)
        duck_add_test_custom(vm ${test_set}_fixed_size_table_test fixed_size_table/fixed_size_table.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_dynamic_table_test dynamic_table/dynamic_table.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_unit_test basic/unit.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_functions_test functions/functions.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_correctness_test correctness/correctness.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt(correctness_test ${test_set})

        duck_add_test_custom(vm ${test_set}_debugger_test debugger/debugger_tests.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_debugger_class_test debugger/debugger_class_tests.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_debugger_infinite_test debugger/debugger_infinite_tests.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt("debugger_infinite_test" ${test_set})

        duck_add_test_custom(vm ${test_set}_LowVMProgramCopy_test debugger/LowVMProgramCopy_tests.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_building_test loader/assembly/bc_building.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_parsing_test loader/assembly/bc_parsing.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_verification_test loader/verification/bc_verification.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_inheritance_loading_test inheritance/inheritance_loading.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_inheritance_semantics_test inheritance/inheritance_semantics.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_inheritance_hierarchy_correctness_test
                inheritance/inheritance_hierarchy_correctness.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_vtable_setting_test inheritance/setting_vtable/vtable_setting.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_code_injection_test injection/code_injection.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_pointers_test pointers/vm_pointers_test.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_variant_test variant/variant_tests.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_extern_c_func_test extern_c/vm_extern_c_func_test.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_threads_test threads/thread_tests.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_when_not_opt("threads_test" ${test_set})

        duck_add_test_custom(vm ${test_set}_jit_test jit/jit.cpp
                USES ${libvm} Tester ${vmtester})

        if (test_set STREQUAL "vm_tc")
                set_property(TEST ${test_set}_jit_test PROPERTY DISABLED TRUE)
        endif()
endfunction()

duck_add_vm_tests("tc")
duck_add_vm_tests("sc")

duck_add_test_custom(vm vm_stable_obj_id_name_map_test utils/stable_obj_id_name_map_test.cpp
        USES LibVM Tester)
duck_add_test_custom(vm vm_memory_test memory/memory_test.cpp
        USES LibVM_sc Tester VmTester_sc)
