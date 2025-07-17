add_library(Clap
	src/clap/clap_class.cpp
	src/clap/clap.hpp
	src/clap/parameter.cpp
	src/clap/parameter.hpp
	src/clap/value_parser.cpp
	src/clap/value_parser.hpp
	src/clap/parsing_result.cpp
	src/clap/parsing_result.hpp
	src/clap/parsing_state.cpp
	src/clap/parsing_state.hpp
	src/clap/param_builder.hpp
	src/clap/param_builder.cpp
	src/clap/exceptions.cpp
	src/clap/exceptions.hpp
	src/clap/help_message_generator.cpp
	src/clap/help_message_generator.hpp
	src/clap/clap_class.hpp
)

make_module(Clap USES Base Printer Filesystem)

duck_add_test(common clap tests/clap_test.cpp USES Clap Base)

duck_add_test(common clap_parser tests/clap_parser_test.cpp USES Clap Base)

add_example(clap_example_cat examples/clap_example_cat.cpp
	USES Clap Base)
