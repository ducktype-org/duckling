add_library(Clap
	src/clap/clap_class.cpp
	src/clap/clap.hpp
	src/clap/parameter.cpp
	src/clap/parameter.hpp
	src/clap/value_parser.cpp
	src/clap/value_parser.hpp
	src/clap/parsing_result.cpp
	src/clap/parsing_result.hpp
	src/clap/param_builder.hpp
	src/clap/param_builder.cpp
	src/clap/exceptions.cpp
	src/clap/exceptions.hpp
	src/clap/help_message_generator.cpp
	src/clap/help_message_generator.hpp
	src/clap/clap_class.hpp
)

make_module(Clap USES Base Filesystem)

rift_add_test(common clap_tester tests/clap_tests.cpp
	USES Clap Base)

rift_add_test(common clap_parser_tester tests/clap_parser_tests.cpp
	USES Clap Base)
