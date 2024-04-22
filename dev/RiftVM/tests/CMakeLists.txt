add_library(RiftVmTester STATIC
        utils/vm_tester_utils.cpp)

target_include_directories(RiftVmTester 
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/utils)

target_link_libraries(RiftVmTester PRIVATE
        LibRiftVm
        Tester)

rift_add_test(vm vm_micro_test basic/working_vm.cpp
        USES LibRiftVm Tester)
rift_add_test(vm vm_static_table_test static_table/static_table.cpp
        USES LibRiftVm Tester RiftVmTester)
rift_add_test(vm vm_unit_tests basic/unit.cpp
        USES LibRiftVm Tester RiftVmTester)
rift_add_test(vm vm_functions_test functions/functions.cpp
        USES LibRiftVm Tester RiftVmTester)

add_subdirectory(performance)
