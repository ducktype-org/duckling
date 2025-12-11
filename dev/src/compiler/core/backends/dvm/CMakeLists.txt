# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time.
add_library(DVMBackend STATIC
    src_private/dvm_backend.cpp
    src_private/program_lowering_context.cpp
    src_private/function_lowering_context.cpp
    src_private/instruction_lowering.cpp
    src_private/dvm_value.cpp
    src_private/dvm_operation.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM Logger)

duck_add_test(compiler dvm_backend tests/dvm_backend_test.cpp USES DVMBackend LibVM VmTester_sc)
