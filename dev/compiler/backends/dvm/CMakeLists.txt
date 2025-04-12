add_library(DVMBackend
    src_private/backend.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM)

duck_add_test(compiler dvm_backend tests/dvm_backend_test.cpp USES DVMBackend LibVM)
