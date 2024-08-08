include(CMakePrintHelpers)

function(add_ducklingvm_tests suffix)
  set(libducklingvm "LibDucklingVm_${suffix}")

  set(vmtester "DucklingVmTester_${suffix}")
  add_library(${vmtester} STATIC
          utils/vm_tester_utils.cpp)
  target_include_directories(${vmtester} 
          PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/utils)
  target_link_libraries(${vmtester} PRIVATE
          ${libducklingvm}
          Tester)

  set(test_set "vm_${suffix}")

  duckling_add_test(vm ${test_set}_micro_test basic/working_vm.cpp
          USES ${libducklingvm} Tester)
  duckling_add_test(vm ${test_set}_static_table_test static_table/static_table.cpp
          USES ${libducklingvm} Tester ${vmtester})
  duckling_add_test(vm ${test_set}_unit_tests basic/unit.cpp
          USES ${libducklingvm} Tester ${vmtester})
  duckling_add_test(vm ${test_set}_functions_test functions/functions.cpp
          USES ${libducklingvm} Tester ${vmtester})
  duckling_add_test(vm ${test_set}_correctness_test correctness/correctness.cpp
          USES ${libducklingvm} Tester ${vmtester})        

  set_property(TEST ${test_set}_correctness_test PROPERTY DISABLED "$<$<CONFIG:Debug>:True>")
endfunction()

add_ducklingvm_tests("tc")
add_ducklingvm_tests("sc")
add_ducklingvm_tests("cg")
