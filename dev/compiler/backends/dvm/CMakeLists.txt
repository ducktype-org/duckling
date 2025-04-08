add_library(DVMBackend
    src_private/backend.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM)

duck_add_test(compiler dvm_backend_unit tests/dvm_backend_unit_test.cpp
    USES Base DVMBackend)
