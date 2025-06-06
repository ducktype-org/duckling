add_library(TypeSystemLower
        src/typesystem/lower/type_layout.cpp
        src/typesystem/lower/queries.cpp
)

make_module(TypeSystemLower USES Base QueryFramework TypeSystemHigher)

duck_add_test(compiler tsl_simple tests/tsl_simple_test.cpp USES TypeSystemLower)
