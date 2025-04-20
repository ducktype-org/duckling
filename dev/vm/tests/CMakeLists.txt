include(CMakePrintHelpers)

# When VM runs code for a long time in TC mode, without the "tailcall" optimization
# enabled, it will run out of stack space.
function(duck_disable_tc_in_debug test_name test_set)
        if(test_set STREQUAL "vm_tc")
                set_property(TEST ${test_set}_${test_name} PROPERTY DISABLED "$<$<CONFIG:Debug>:True>")
        endif()
endfunction()

function(duck_add_vm_tests suffix)
        set(libvm "LibVM_${suffix}")

        set(vmtester "VmTester_${suffix}")
        add_library(${vmtester} STATIC
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
        duck_add_test_custom(vm ${test_set}_static_table_test static_table/static_table.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_unit_tests basic/unit.cpp
                USES ${libvm} Tester ${vmtester})
        duck_add_test_custom(vm ${test_set}_functions_test functions/functions.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_correctness_test correctness/correctness.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_in_debug(correctness_test ${test_set})

        duck_add_test_custom(vm ${test_set}_debugger_test debugger/debugger_tests.cpp
                USES ${libvm} Tester ${vmtester})

        duck_add_test_custom(vm ${test_set}_debugger_infinite_test debugger/debugger_infinite_tests.cpp
                USES ${libvm} Tester ${vmtester})
        duck_disable_tc_in_debug("debugger_infinite_test" ${test_set})

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
# @TODOB
# <<<<<<< HEAD
#         duck_add_test_custom(vm ${test_set}_parsing_test parsing/bc_parsing.cpp
#             USES ${libvm} Tester ${vmtester})
#         # @TODO verification (for now) happens before loading the parsed code,
#         # so we got the wrong errors in these tests, this will probably change with loaders,
#         # so im just lazily disabling them
#         set_property(TEST ${test_set}_parsing_test PROPERTY DISABLED TRUE)
# 
#         duck_add_test_custom(vm ${test_set}_validation_test validation/bc_validation.cpp
#                 USES ${libvm} Tester ${vmtester})
# 
#         duck_add_test_custom(vm ${test_set}_inheritance_loading_test inheritance/inheritance_loading.cpp
#             USES ${libvm} Tester ${vmtester})
#         duck_add_test_custom(vm ${test_set}_inheritance_hierarchy_correctness_test
#             inheritance/inheritance_hierarchy_correctness.cpp
#             USES ${libvm} Tester ${vmtester})
#         duck_add_test_custom(vm ${test_set}_inheritance_semantics_test inheritance/inheritance_semantics.cpp
# =======
endfunction()

duck_add_vm_tests("tc")
duck_add_vm_tests("sc")
duck_add_vm_tests("cg")

duck_add_test_custom(vm ${test_set}_stable_type_id_name_map_test utils/stable_type_id_name_map_test.cpp
        USES LibVM Tester)
