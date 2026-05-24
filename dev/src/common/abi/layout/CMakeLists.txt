add_library(AbiLayout
	src/abi/layout/target.cpp
	src/abi/layout/compute_c_layout.cpp
)

make_module(AbiLayout USES Base AbiTypeSystem)

duck_add_test(common abi_layout tests/abi_layout_test.cpp USES AbiLayout)
