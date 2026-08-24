# This has to be static due to cyclic dependencies in helios->mir->lir->dvm for comp time.
add_library(MIR STATIC
	src_private/mir_private/stmt_lowering.cpp
	src_private/mir_private/expr_lowering.cpp
	src_private/mir_private/mir_builders.cpp
	src_private/mir_private/utils/bounds_check.cpp

	src/mir/mir_lowering/mir_lifetimes.cpp
	src/mir/mir_lowering/mir_liveness.cpp
	src/mir/mir_lowering/mir_validation.cpp
	src/mir/mir_lowering/mir_queries.cpp
	src/mir/mir_lowering/mir_unit.cpp
	src/mir/mir_structure/mir_structure.cpp
	src/mir/mir_structure/mir_lifetime_scope.cpp
)

make_module(MIR USES Base QueryFramework HELIOS)

duck_add_test(compiler mir tests/mir_test.cpp USES Base PstParser HELIOS MIR)
duck_add_test(compiler mir_with_std tests/mir_with_std_test.cpp USES Base PstParser HELIOS MIR Driver)
duck_add_test(compiler mir_errors tests/mir_errors_test.cpp USES Base PstParser HELIOS MIR)
duck_add_test(compiler mir_lifetime tests/mir_lifetime_test.cpp USES Base PstParser HELIOS MIR)
