add_library(Clap
		src/clap/clap.cpp
		src/clap/error.cpp
		src/clap/parameters_map.cpp
		src/clap/parameter_config.cpp)

make_module(Clap USES Base Filesystem)

rift_add_test(common clap_test_simple tests/test.cpp
		USES Clap Base)
