add_library(Base
		src/base/impl/define_helper.cpp
		src/base/impl/exceptions.cpp
		src/base/impl/raw_view.cpp
		src/base/impl/string_id.cpp
		src/base/impl/str_concat.cpp
		src/base/impl/convert.cpp
		src/base/str_replace.hpp
)

set_target_properties(Base PROPERTIES LINKER_LANGUAGE CXX)


# @TODO: unicode depdencdy might not be optinal. 
# Think of a way to "break" it 
make_module(Base USES Result::Result unicode)

rift_add_test(base base_test_simple tests/test.cpp
		USES Printer Base)

rift_add_test(base variant_test_simple tests/variant_test.cpp
		USES Base Tester)

rift_add_test(base test_flag_type tests/flag_type_test.cpp
		USES Base)

rift_add_test(base stable_container_test_simple tests/stable_container_test.cpp
		USES Base)

rift_add_test(base test_smart_pointer tests/pointer_tests.cpp
		USES Base)

rift_add_test(base string_id_test_simple tests/string_id_test.cpp
		USES Lexer Base)

rift_add_test(base optional_tests tests/optional_test.cpp
		USES Base Tester)
