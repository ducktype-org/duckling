add_library(CTV STATIC
    src/ctv/ctv.cpp
    src/ctv/numeric_value.cpp
)

make_module(CTV USES Base HELIOS)

duck_add_test(compiler ctv tests/ctv_test.cpp USES CTV Base)
