#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/stmt_specifier.hpp>
#include <frontend/pst_parser/pst_query/code_dependency.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <helios/helios_errors.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_class_symbol_data.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/highlight_positions.hpp>
#include <filesystem/file.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// TESTER_ADD_TEST(testImport);
		// TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testConstants);
		// TESTER_ADD_TEST(testNumericLiterals);
		// TESTER_ADD_TEST(testClassSymbolData);
		// TESTER_ADD_TEST(testClassInteractions);
		// TESTER_ADD_TEST(testTypeInstanceInterface);
		// TESTER_ADD_TEST(testHoutVariables);
		// TESTER_ADD_TEST(testExprTree);
		// TESTER_ADD_TEST(testExprClone);
		// TESTER_ADD_TEST(testSimpleHOUT);
		// TESTER_ADD_TEST(testSingleFileModuleHOUT);
		// TESTER_ADD_TEST(testModuleHOUT);
		// TESTER_ADD_TEST(testDependencyHOUT);
		// TESTER_ADD_TEST(testHoutVisitor);
		// TESTER_ADD_TEST(testTypeOf);
		// TESTER_ADD_TEST(testKeywordLiterals);
		// TESTER_ADD_TEST(testFunctionParameters);
		// TESTER_ADD_TEST(testExprScopes);
		// TESTER_ADD_TEST(testFunctionCallExpr);
		// TESTER_ADD_TEST(testFunctions);
		// TESTER_ADD_TEST(testBuiltinFunctions);
		// TESTER_ADD_TEST(testMangler);
		// TESTER_ADD_TEST(testManglerSpecialMembers);
		// TESTER_ADD_TEST(testGlobalVariableExpressions);
		// TESTER_ADD_TEST(testTypeOfConstAndVar);
		// TESTER_ADD_TEST(testDebugPrint);
		// TESTER_ADD_TEST(testStmtSpecifiers);
		// TESTER_ADD_TEST(testOverloadResolution);
		// TESTER_ADD_TEST(testCastsHout);
		// TESTER_ADD_TEST(testTypeLifting);

		// error tests
		// TESTER_ADD_TEST(testErrorBadExpr);
		// TESTER_ADD_TEST(testErrorAmbiguousCallableCandidates);

		// this is at the end
		// so we test all the scopes created in helios tests:
		// TESTER_ADD_TEST(testScopeParentsAndDepth);
		// TESTER_ADD_TEST(testScopeSymbolsConsistency);
	}

private:
	// @TODO: test_modules/aliases are not used in tests

	using enum compiler::tsh::Mutability;
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	/**
	 * Shorthand to create a mutable symbol type from an abstract type.
	 */
	static compiler::tsh::SymbolType<> st(const compiler::tsh::AbstractType abstract_type) {
		return compiler::tsh::SymbolType{
			abstract_type,
			compiler::tsh::ReferenceKind::Direct,
			Mutable,
		};
	}

	void testConstants() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/constants")));

		// ASSERT_EQUAL(1'107, getConstValueAs<i64>("M", root_scope));
		// ASSERT_EQUAL(1, getConstValueAs<i32>("N.X", root_scope));
		// ASSERT_EQUAL(1, getConstValueAs<i32>("A", root_scope));
		// ASSERT_EQUAL(-3, getConstValueAs<i32>("B", root_scope));
		// ASSERT_EQUAL(-1, getConstValueAs<i64>("D", root_scope));
		// ASSERT_EQUAL(6, getConstValueAs<i32>("E", root_scope));
		// ASSERT_EQUAL(std::numeric_limits<i32>::max(), getConstValueAs<i32>("MAX_I32", root_scope));
		// ASSERT_EQUAL(3, getConstValueAs<i64>("H2", root_scope));
		// ASSERT_EQUAL(1, getConstValueAs<i64>("T0", root_scope));
		// ASSERT_EQUAL(2, getConstValueAs<i64>("T1", root_scope));
		// ASSERT_EQUAL(3, getConstValueAs<i64>("T2", root_scope));
		// ASSERT_EQUAL(30, getConstValueAs<i64>("F", root_scope));

		// Floating point.
		// ASSERT_EQUAL(1.0f, getConstValueAs<f64>("F1", root_scope));
		// ASSERT_EQUAL(1.0l, getConstValueAs<f32>("F2", root_scope));
		// ASSERT_EQUAL(5.0l, getConstValueAs<f64>("F3", root_scope));

		// ASSERT_EQUAL(true, getConstValueAs<bool>("BOOL_TRUE", root_scope));
		// ASSERT_EQUAL(false, getConstValueAs<bool>("BOOL_FALSE", root_scope));
		// ASSERT_EQUAL(true, getConstValueAs<bool>("LOGIC_AND", root_scope));
		// ASSERT_EQUAL(false, getConstValueAs<bool>("LOGIC_OR", root_scope));
		// ASSERT_EQUAL(true, getConstValueAs<bool>("TRUE_COMPARISON", root_scope));
		// ASSERT_EQUAL(false, getConstValueAs<bool>("FALSE_COMPARISON", root_scope));

		// ASSERT_EQUAL(42, getConstValueAs<i64>("VM_SIMPLE_CALL", root_scope));
		// ASSERT_EQUAL(1'129, getConstValueAs<i64>("VM_SIMPLE_CALL_2", root_scope));
		// ASSERT_EQUAL(55, getConstValueAs<i64>("FIB_10", root_scope));
		// getConstValueAs<i64>();
		getConstValue("COMPLEX_VM_CALL", root_scope);
		// ASSERT_EQUAL(37, getConstValueAs<i64>("COMPLEX_VM_CALL_2", root_scope));
		// ASSERT_EQUAL(1, getConstValueAs<i64>("COLLATZ", root_scope));
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
