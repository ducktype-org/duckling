add_library(RiftDefinitions 
            src/rift_definitions/key_spec_op.cpp
            src/rift_definitions/operator_precedence.cpp)

make_module(RiftDefinitions USES Base)

rift_add_test(common rift_def_test_simple tests/test.cpp
	USES RiftDefinitions)
