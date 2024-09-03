include(CMakePrintHelpers)

function(add_vm_tests suffix)
  set(libvm "LibVm_${suffix}")

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

  cmake_add_test(vm ${test_set}_micro_test basic/working_vm.cpp
          USES ${libvm} Tester)
  cmake_add_test(vm ${test_set}_static_table_test static_table/static_table.cpp
          USES ${libvm} Tester ${vmtester})
  cmake_add_test(vm ${test_set}_unit_tests basic/unit.cpp
          USES ${libvm} Tester ${vmtester})
  cmake_add_test(vm ${test_set}_functions_test functions/functions.cpp
          USES ${libvm} Tester ${vmtester})
  cmake_add_test(vm ${test_set}_correctness_test correctness/correctness.cpp
          USES ${libvm} Tester ${vmtester})        

  set_property(TEST ${test_set}_correctness_test PROPERTY DISABLED "$<$<CONFIG:Debug>:True>")
endfunction()

add_vm_tests("tc")
add_vm_tests("sc")
add_vm_tests("cg")
