add_library(DVMBackend
    src_private/backend.cpp
    src_private/get_parameter_types.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM)

duck_add_test(compiler dvm_backend tests/dvm_backend_test.cpp USES DVMBackend LibVM VmTester_tc)
