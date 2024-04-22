add_library(RiftVmTester STATIC
        utils/vm_tester_utils.cpp)

target_include_directories(RiftVmTester 
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/utils)

function(add_riftvm_tests suffix)
  set(libriftvm "LibRiftVm_${suffix}")

  target_link_libraries(RiftVmTester PRIVATE
          ${libriftvm}
          Tester)

  set(test_set "vm_${suffix}")

  rift_add_test(vm ${test_set}_micro_test basic/working_vm.cpp
          USES ${libriftvm} Tester)
  rift_add_test(vm ${test_set}_static_table_test static_table/static_table.cpp
          USES ${libriftvm} Tester RiftVmTester)
  rift_add_test(vm ${test_set}_unit_tests basic/unit.cpp
          USES ${libriftvm} Tester RiftVmTester)
  rift_add_test(vm ${test_set}_functions_test functions/functions.cpp
          USES ${libriftvm} Tester RiftVmTester)
  rift_add_test(vm ${test_set}_correctness_test correctness/correctness.cpp
          USES ${libriftvm} Tester RiftVmTester)
endfunction()

add_riftvm_tests("tc")
add_riftvm_tests("sc")
add_riftvm_tests("cg")
