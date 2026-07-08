# Shared object holding the native functions the FFI tests call into.
add_library(ffi_test_lib SHARED ffi_test_lib.cpp)
set_target_properties(ffi_test_lib PROPERTIES EXCLUDE_FROM_ALL true)

duck_add_test_custom(vm vm_ffi_test vm_ffi_test.cpp
        USES LibVM_sc Tester VmTester_sc)

# The test bakes in the absolute path of the built shared object and loads it at runtime.
target_compile_definitions(vm_ffi_test PRIVATE
        FFI_TEST_LIB_PATH="$<TARGET_FILE:ffi_test_lib>")
add_dependencies(vm_ffi_test ffi_test_lib)
