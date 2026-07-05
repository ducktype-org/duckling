# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time.
add_library(DVMBackend STATIC
    src_private/program_lowering_context.cpp
    src_private/function_lowering_context.cpp
    src_private/ctv_lowering.cpp
    src_private/instruction_lowering.cpp
    src_private/dvm_value.cpp
    src_private/operations/dvm_operation.cpp
    src_private/operations/meta.cpp
    src_private/operations/terminators.cpp
    src_private/operations/comparison.cpp
    src_private/operations/cast.cpp
    src_private/operations/call.cpp
    src_private/operations/arithmetic.cpp
    src_private/operations/address_of.cpp
    src_private/operations/box.cpp
    src_private/operations/variant.cpp
    src/backends/dvm/repl_lowering.cpp
    src/backends/dvm/dvm_backend.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM Logger DebugInfo)

duck_add_test(compiler dvm_backend tests/dvm_backend_test.cpp USES DVMBackend LibVM VmTester_sc Driver)
