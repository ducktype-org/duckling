add_library(MIR
	src_private/mir_private/stmt_lowering.cpp
	src_private/mir_private/expr_lowering.cpp
	src_private/mir_private/mir_builders.cpp

	src/mir/mir_lowering/mir_lifetimes.cpp
	src/mir/mir_lowering/mir_validation.cpp
	src/mir/mir_lowering/mir_queries.cpp
	src/mir/mir_structure/mir_structure.cpp
	src/mir/mir_structure/mir_lifetime_scope.cpp
)

make_module(MIR USES Base QueryFramework HELIOS)

duck_add_test(compiler mir tests/mir_test.cpp USES Base TypeSystemHigher PstParser HELIOS MIR)
