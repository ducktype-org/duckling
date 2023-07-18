add_library(Config 
            src/config/config.cpp
            src/config/parsing_result.cpp
            src/config/params_configuration.cpp
            src/config/value_parser.cpp)

make_module(Config USES Base Filesystem Printer)

rift_add_test(common config_test_simple tests/test.cpp
	USES Config Base)
