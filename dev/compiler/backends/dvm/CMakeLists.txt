add_library(DVMBackend
    src/backends/dvm/backend.cpp
    src/backends/dvm/builders.cpp
    src/backends/dvm/instruction_builder.cpp
    src/backends/dvm/serializer.cpp
    src/backends/dvm/utils.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR Init LibVM_tc)

duck_add_test(compiler dvm_backend_unit tests/dvm_backend_unit_test.cpp
	USES Base DVMBackend)
