rift_add_test(vm vm_micro_test micro_test.cpp
        USES LibRiftVm Tester)
rift_add_test(vm vm_static_table_test static_table.cpp
        USES LibRiftVm Tester)
rift_add_test(vm vm_unit_tests unit.cpp
        USES LibRiftVm Tester)

add_subdirectory(performance)
