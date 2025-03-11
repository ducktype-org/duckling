add_library(Base
        src/base/impl/define_helper.cpp
        src/base/impl/exceptions.cpp
        src/base/impl/raw_view.cpp
        src/base/impl/string_id.cpp
        src/base/impl/convert.cpp
        src/base/impl/str_utils.cpp
)

set_target_properties(Base PROPERTIES LINKER_LANGUAGE CXX)

# @TODO: unicode dependency might not be optional.
# Think of a way to "break" it
make_module(Base USES Result::Result unicode)

duck_add_test(base strongly_typed_int USES Base)

duck_add_test(base variant USES Base)

duck_add_test(base flag_type USES Base)

duck_add_test(base stable_container USES Base)

duck_add_test(base string_id USES Base)

duck_add_test(base optional USES Base)

duck_add_test(base str_concat USES Base)

duck_add_test(base constexpr_cat USES Base)

duck_add_test(base view USES Base)

duck_add_test(base exception USES Base)

duck_add_test(base defer USES Base)

duck_add_test(base strong_id USES Base)

duck_add_test(base at_maybe USES Base)

duck_add_test(base perfect_hash USES Base)

duck_add_test(base box_ref USES Base)

duck_add_test(base bits_and_bytes USES Base)

duck_add_test(base is_complete USES Base)

duck_add_test(base type_name USES Base)

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
