# Shared object holding the native functions the FFI tests call into.
add_library(ffi_test_lib SHARED ffi_test_lib.cpp)
set_target_properties(ffi_test_lib PROPERTIES EXCLUDE_FROM_ALL true)

# Bakes the absolute path of `ffi_test_lib` into an FFI test, which loads it at runtime, and makes
# sure the library is built before the test.
function(duck_use_ffi_test_lib test_name)
        target_compile_definitions(${test_name} PRIVATE
                FFI_TEST_LIB_PATH="$<TARGET_FILE:ffi_test_lib>")
        add_dependencies(${test_name} ffi_test_lib)
endfunction()
