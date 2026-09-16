add_library(Clah
	src/clah/clah_class.cpp
	src/clah/clah.hpp
	src/clah/parameter.cpp
	src/clah/parameter.hpp
	src/clah/value_parser.cpp
	src/clah/value_parser.hpp
	src/clah/parsing_result.cpp
	src/clah/parsing_result.hpp
	src/clah/parsing_state.cpp
	src/clah/parsing_state.hpp
	src/clah/positional_parameter.hpp
	src/clah/param_builder.hpp
	src/clah/param_builder.cpp
	src/clah/exceptions.cpp
	src/clah/exceptions.hpp
	src/clah/help_message_generator.cpp
	src/clah/help_message_generator.hpp
	src/clah/clah_class.hpp
)

make_module(Clah USES Base Printer Filesystem)

duck_add_test(common clah tests/clah_test.cpp USES Clah Base)

duck_add_test(common clah_parser tests/clah_parser_test.cpp USES Clah Base)

add_example(clah_example_cat examples/clah_example_cat.cpp
	USES Clah Base)
