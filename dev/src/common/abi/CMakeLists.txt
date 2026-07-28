add_library(ForeignAbiUtils
	src/abi/type_system/type.cpp
	src/abi/target.cpp
	src/abi/layout/compute_c_layout.cpp
	src/abi/calling_conv/calling_conv.cpp
)

make_module(ForeignAbiUtils USES Base)


duck_add_test(common abi_layout tests/abi_layout_test.cpp USES ForeignAbiUtils)
duck_add_test(common abi_calling_conv tests/abi_calling_conv_test.cpp USES ForeignAbiUtils)
