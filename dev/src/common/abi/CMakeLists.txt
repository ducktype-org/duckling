add_library(ForeignAbiUtils
	src/abi/type_system/type.cpp
	src/abi/layout/target.cpp
	src/abi/layout/compute_c_layout.cpp
)

make_module(ForeignAbiUtils USES Base)


duck_add_test(common abi_layout tests/abi_layout_test.cpp USES ForeignAbiUtils)
