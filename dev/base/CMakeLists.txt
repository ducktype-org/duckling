add_library(Base
        src/base/impl/define_helper.cpp
        src/base/impl/exceptions.cpp
        src/base/impl/raw_view.cpp
        src/base/impl/string_id.cpp
        src/base/impl/convert.cpp
        src/base/impl/str_utils.cpp
        src/base/impl/init_guards.cpp
)

set_target_properties(Base PROPERTIES LINKER_LANGUAGE CXX)

# @TODO: unicode dependency might not be optional.
# Think of a way to "break" it
make_module(Base USES Result::Result unicode)

duck_add_test(base strongly_typed_int_tests tests/strongly_typed_int_test.cpp
        USES Base)

duck_add_test(base variant_test_simple tests/variant_test.cpp
        USES Base)

duck_add_test(base test_flag_type tests/flag_type_test.cpp
        USES Base)

duck_add_test(base stable_container_test_simple tests/stable_container_test.cpp
        USES Base)

duck_add_test(base test_smart_pointer tests/pointer_tests.cpp
        USES Base)

duck_add_test(base string_id_test_simple tests/string_id_test.cpp
        USES Base)

duck_add_test(base optional_tests tests/optional_test.cpp
        USES Base)

duck_add_test(base str_concat_tests tests/str_concat_test.cpp
        USES Base)

duck_add_test(base constexpr_cat_tests tests/constexpr_cat_test.cpp
        USES Base)

duck_add_test(base view_tests tests/view_test.cpp
        USES Base)

duck_add_test(base exception_tests tests/exception_test.cpp
        USES Base)

duck_add_test(base defer_tests tests/defer_test.cpp
        USES Base)

duck_add_test(base strong_id_test tests/strong_id_test.cpp
        USES Base)

duck_add_test(base at_maybe_test tests/at_maybe_test.cpp
        USES Base)

duck_add_test(base perfect_hash_test tests/perfect_hash_test.cpp
        USES Base)

duck_add_test(base box_ref_tests tests/box_ref_tests.cpp
        USES Base)

add_example(anycast_example src/base/examples/anycast_example.cpp
        USES Base)

add_example(constexpr_cat_example src/base/examples/constexpr_cat_example.cpp
        USES Base)

add_example(defer_example src/base/examples/defer_example.cpp
        USES Base)

add_example(optional_simple_example src/base/examples/optional_simple_example.cpp
        USES Base)

add_example(perfect_hash_example src/base/examples/perfect_hash_example.cpp
        USES Base)

add_example(smart_pointers_example src/base/examples/smart_pointers_example.cpp
        USES Base)

add_example(str_utils_example src/base/examples/str_utils_example.cpp
        USES Base)

add_example(stringifyable_enum_example src/base/examples/stringifyable_enum_example.cpp
        USES Base)

add_example(strongly_typed_int_example src/base/examples/strongly_typed_int_example.cpp
        USES Base)

add_example(variant_match_example src/base/examples/variant_match_example.cpp
        USES Base)

add_example(variant_visit_example src/base/examples/variant_visit_example.cpp
        USES Base)

duck_add_test(base bits_and_bytes_test tests/bits_and_bytes_test.cpp
        USES Base)

duck_add_test(base is_complete_test tests/is_complete_test.cpp
        USES Base)

duck_add_test(base type_name_test tests/type_name_test.cpp
        USES Base)
