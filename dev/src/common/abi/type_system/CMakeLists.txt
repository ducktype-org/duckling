add_library(AbiTypeSystem
	src/abi/type_system/type.cpp
)

make_module(AbiTypeSystem USES Base)

duck_add_test(common abi_type_system tests/abi_type_system_test.cpp USES AbiTypeSystem Concurrent)
