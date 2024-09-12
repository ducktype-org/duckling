add_subdirectory(higher)

# @FIXME: adding HELIOS dependency for SymID introduces cyclic dependency (but everything compiles).
make_module(TypeSystem USES Base HELIOS QueryFramework)

rift_add_test(compiler typesystem_test_simple tests/simple_test.cpp USES TypeSystem)

rift_add_test(compiler typesystem_test_error tests/error_test.cpp USES TypeSystem)

rift_add_test(compiler typesystem_test_overload_resolution tests/overload_resolution_test.cpp USES TypeSystem HELIOS)
