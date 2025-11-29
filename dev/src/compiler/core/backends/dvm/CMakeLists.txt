# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time.
add_library(DVMBackend STATIC
    src_private/backend.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM Logger)

duck_add_test(compiler dvm_backend tests/dvm_backend_test.cpp USES DVMBackend LibVM VmTester_sc)
