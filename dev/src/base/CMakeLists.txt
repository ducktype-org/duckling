add_library(Base
        src/base/preproc/argument_splitter.cpp
        
        src/base/except/exceptions.cpp
        src/base/except/pretty_stacktrace.cpp
        
        src/base/misc/raw_view.cpp
        src/base/misc/convert.cpp
        src/base/misc/init_guards.cpp
        
        src/base/str/str_utils.cpp
        
        src/base/types/bit256.cpp
        src/base/types/checked_okbad.cpp
)

# @TODO: unicode dependency might not be optimal.
# Think of a way to "break" it
make_module(Base USES unicode system_stdcxxexp)

duck_add_test(base strongly_typed_int tests/strongly_typed_int_test.cpp USES Base)

duck_add_test(base variant tests/variant_test.cpp USES Base)

duck_add_test(base flag_type tests/flag_type_test.cpp USES Base)

duck_add_test(base stable_container tests/stable_container_test.cpp USES Base)


duck_add_test(base optional tests/optional_test.cpp USES Base)

duck_add_test(base str_concat tests/str_concat_test.cpp USES Base)

duck_add_test(base constexpr_cat tests/constexpr_cat_test.cpp USES Base)

duck_add_test(base view tests/view_test.cpp USES Base)

duck_add_test(base exception tests/exception_test.cpp USES Base)

duck_add_test(base defer tests/defer_test.cpp USES Base)

duck_add_test(base strong_id tests/strong_id_test.cpp USES Base)

duck_add_test(base at_maybe tests/at_maybe_test.cpp USES Base)

duck_add_test(base box_ref tests/box_ref_test.cpp USES Base)

duck_add_test(base bits_and_bytes tests/bits_and_bytes_test.cpp USES Base)

duck_add_test(base is_complete tests/is_complete_test.cpp USES Base)

duck_add_test(base type_name tests/type_name_test.cpp USES Base)

duck_add_test(base ok_bad tests/ok_bad_test.cpp USES Base)

duck_add_test(base str_enum tests/str_enum_test.cpp USES Base)

duck_add_test(base bit256 tests/bit256_test.cpp USES Base)

duck_add_test(base memory tests/memory_test.cpp USES Base)

duck_add_test(base map tests/map_test.cpp USES Base)

duck_add_test(base numeric_utils tests/numeric_utils_test.cpp USES Base)

duck_add_test(base base tests/base_test.cpp USES Base)

duck_add_test(base shared_box tests/shared_box_test.cpp USES Base)

duck_add_test(base vector_utils tests/vector_utils_test.cpp USES Base)

add_example(anycast_example examples/anycast_example.cpp
        USES Base)

add_example(constexpr_cat_example examples/constexpr_cat_example.cpp
        USES Base)

add_example(defer_example examples/defer_example.cpp
        USES Base)

add_example(optional_simple_example examples/optional_simple_example.cpp
        USES Base)

add_example(stringifyable_enum_example examples/stringifyable_enum_example.cpp
        USES Base)

add_example(strongly_typed_int_example examples/strongly_typed_int_example.cpp
        USES Base)

add_example(variant_match_example examples/variant_match_example.cpp
        USES Base)

add_example(variant_visit_example examples/variant_visit_example.cpp
        USES Base)
