#include <diagnostic_interactive/logger.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/pst_query/code_dependency.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_class_symbol_data.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/mutability.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/highlight_positions.hpp>
#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/internal/query_errors.hpp>
#include <query_framework/query_result.hpp>
#include <tester/tester.hpp>

using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testImport);
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testConstants);
		TESTER_ADD_TEST(testMetaCompTime);
		TESTER_ADD_TEST(testNumericLiterals);
		TESTER_ADD_TEST(testClassSymbolData);
		TESTER_ADD_TEST(testClassInteractions);
		TESTER_ADD_TEST(testTypeInstanceInterface);
		TESTER_ADD_TEST(testHoutVariables);
		TESTER_ADD_TEST(testReferences);
		TESTER_ADD_TEST(testBoxes);
		TESTER_ADD_TEST(testReferenceKindCollapsing);
		TESTER_ADD_TEST(testExprTree);
		TESTER_ADD_TEST(testExprClone);
		TESTER_ADD_TEST(testSimpleHOUT);
		TESTER_ADD_TEST(testSingleFileModuleHOUT);
		TESTER_ADD_TEST(testModuleHOUT);
		TESTER_ADD_TEST(testDependencyHOUT);
		TESTER_ADD_TEST(testHoutVisitor);
		TESTER_ADD_TEST(testTypeOf);
		TESTER_ADD_TEST(testKeywordLiterals);
		TESTER_ADD_TEST(testFunctionParameters);
		TESTER_ADD_TEST(testExprScopes);
		TESTER_ADD_TEST(testFunctionCallExpr);
		TESTER_ADD_TEST(testFunctions);
		TESTER_ADD_TEST(testStrings);
		TESTER_ADD_TEST(testStaticArrays);
		TESTER_ADD_TEST(testDynamicArrays);
		TESTER_ADD_TEST(testBuiltinFunctions);
		TESTER_ADD_TEST(testFunctionReturnTypeDeduction);
		TESTER_ADD_TEST(testFunctionReturnTypeCheckAndCoercion);
		TESTER_ADD_TEST(testMethodCalls);
		TESTER_ADD_TEST(testMangler);
		TESTER_ADD_TEST(testManglerSpecialMembers);
		TESTER_ADD_TEST(testGlobalVariableExpressions);
		TESTER_ADD_TEST(testTypeOfConstAndVar);
		TESTER_ADD_TEST(testDebugPrint);
		TESTER_ADD_TEST(testStmtSpecifiers);
		TESTER_ADD_TEST(testOverloadResolution);
		TESTER_ADD_TEST(testDefaultInitializers);
		TESTER_ADD_TEST(testCastsHout);
		TESTER_ADD_TEST(testTypeLifting);
		TESTER_ADD_TEST(testHoutElementsOrigin);

		// this is at the end
		// so we test all the scopes created in helios tests:
		TESTER_ADD_TEST(testScopeParentsAndDepth);
		TESTER_ADD_TEST(testScopeSymbolsConsistency);
	}

private:
	// @TODO: test_modules/aliases are not used in tests

	using enum compiler::tsh::Mutability;
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getIntegralTypeNoContext(
		u64 size, compiler::tsh::IntegralAbstractType::Signedness signedness
	) {
		return std::any_cast<compiler::tsh::IntegralAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return compiler::tsh::getIntegralType(ctx, size, signedness);
			})
		);
	}

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getFloatTypeNoContext(u64 size) {
		return std::any_cast<compiler::tsh::FloatAbstractType>(query::utils::withContextCompute(
			[&](query::Context& ctx) { return compiler::tsh::getFloatType(ctx, size); }
		));
	}

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

	/**
	 * Shorthand to create a reference to mutable symbol type from an abstract type.
	 */
	static compiler::tsh::SymbolType<> refst(const compiler::tsh::AbstractType abstract_type) {
		return compiler::tsh::SymbolType{
			abstract_type,
			compiler::tsh::ReferenceKind::Ref,
			Mutable,
		};
	}

	void testConstants() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/constants")));

		ASSERT_EQUAL(1'107, getConstValueAs<i64>("M", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i32>("N.X", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i32>("A", root_scope));
		ASSERT_EQUAL(-3, getConstValueAs<i32>("B", root_scope));
		ASSERT_EQUAL(-1, getConstValueAs<i64>("D", root_scope));
		ASSERT_EQUAL(6, getConstValueAs<i32>("E", root_scope));
		ASSERT_EQUAL(27, getConstValueAs<i32>("MOD", root_scope));
		ASSERT_EQUAL(std::numeric_limits<i32>::max(), getConstValueAs<i32>("MAX_I32", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("H2", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("T0", root_scope));
		ASSERT_EQUAL(2, getConstValueAs<i64>("T1", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("T2", root_scope));
		ASSERT_EQUAL(30, getConstValueAs<i64>("F", root_scope));

		// Floating point.
		ASSERT_EQUAL(1.0f, getConstValueAs<f64>("F1", root_scope));
		ASSERT_EQUAL(1.0l, getConstValueAs<f32>("F2", root_scope));
		ASSERT_EQUAL(5.0l, getConstValueAs<f64>("F3", root_scope));

		ASSERT_EQUAL(true, getConstValueAs<bool>("BOOL_TRUE", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("BOOL_FALSE", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("LOGIC_AND", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("LOGIC_OR", root_scope));
		ASSERT_EQUAL(true, getConstValueAs<bool>("TRUE_COMPARISON", root_scope));
		ASSERT_EQUAL(false, getConstValueAs<bool>("FALSE_COMPARISON", root_scope));

		ASSERT_EQUAL(42, getConstValueAs<i64>("VM_SIMPLE_CALL", root_scope));
		ASSERT_EQUAL(1'129, getConstValueAs<i64>("VM_SIMPLE_CALL_2", root_scope));
		ASSERT_EQUAL(55, getConstValueAs<i64>("FIB_10", root_scope));
		ASSERT_EQUAL(55, getConstValueAs<i32>("FIB_ON_I32_10", root_scope));
		ASSERT_EQUAL(58, getConstValueAs<i64>("COMPLEX_VM_CALL", root_scope));
		ASSERT_EQUAL(37, getConstValueAs<i64>("COMPLEX_VM_CALL_2", root_scope));
		ASSERT_EQUAL(1, getConstValueAs<i64>("COLLATZ", root_scope));
	}

	void testMetaCompTime() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/meta_comp_time")));

		auto i16_type  = getIntegralTypeNoContext(16, Signed);
		auto i32_type  = getIntegralTypeNoContext(32, Signed);
		auto i64_type  = getIntegralTypeNoContext(64, Signed);
		auto i128_type = getIntegralTypeNoContext(128, Signed);

		auto f16_type  = getFloatTypeNoContext(16);
		auto f64_type  = getFloatTypeNoContext(64);
		auto f128_type = getFloatTypeNoContext(128);

		auto unit_type = compiler::tsh::getUnitType();

		// Tree eval
		{
			{
				auto simple_ref
					= getConstValueAs<compiler::tsh::SymbolType<>>("SIMPLE_REF", root_scope);
				auto expected = st(i64_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
				ASSERT_EQUAL(expected, simple_ref);
			}
			{
				auto simple_box
					= getConstValueAs<compiler::tsh::SymbolType<>>("SIMPLE_BOX", root_scope);
				auto expected = st(i64_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);
				ASSERT_EQUAL(expected, simple_box);
			}
			{
				auto simple_const
					= getConstValueAs<compiler::tsh::SymbolType<>>("SIMPLE_CONST", root_scope);
				auto expected = st(i64_type).withMutability(Immutable);
				ASSERT_EQUAL(expected, simple_const);
			}
			{
				auto simple_variant
					= getConstValueAs<compiler::tsh::SymbolType<>>("SIMPLE_VARIANT", root_scope);
				auto expected
					= st(query::entryPoint<compiler::tsh::QueryVariantType>({ { st(i32_type),
				                                                                st(f64_type) } }));
				ASSERT_EQUAL(expected, simple_variant);
			}
			{
				auto simple_tuple
					= getConstValueAs<compiler::tsh::SymbolType<>>("SIMPLE_TUPLE", root_scope);
				auto expected
					= st(query::entryPoint<compiler::tsh::QueryTupleType>({ { st(i32_type),
				                                                              st(f64_type) } }));
				ASSERT_EQUAL(expected, simple_tuple);
			}
			{
				auto cmp_1 = getConstValueAs<bool>("CMP_1", root_scope);
				ASSERT_EQUAL(cmp_1, true);
				auto cmp_2 = getConstValueAs<bool>("CMP_2", root_scope);
				ASSERT_EQUAL(cmp_2, true);
			}
		}

		// Function evaluation.
		{
			{
				auto a_type   = getConstValueAs<compiler::tsh::SymbolType<>>("A", root_scope);
				auto expected = st(unit_type);
				ASSERT_EQUAL(expected, a_type);
			}
			{
				auto b_type   = getConstValueAs<compiler::tsh::SymbolType<>>("B", root_scope);
				auto expected = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);
				ASSERT_EQUAL(expected, b_type);
			}
			{
				auto c_type   = getConstValueAs<compiler::tsh::SymbolType<>>("C", root_scope);
				auto expected = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
				ASSERT_EQUAL(expected, c_type);
			}
			{
				auto d_type   = getConstValueAs<compiler::tsh::SymbolType<>>("D", root_scope);
				auto expected = st(i32_type).withMutability(compiler::tsh::Mutability::Immutable);
				ASSERT_EQUAL(expected, d_type);
			}
			{
				auto e_type   = getConstValueAs<compiler::tsh::SymbolType<>>("E", root_scope);
				auto expected = query::entryPoint<compiler::tsh::QueryVariantType>(
					{ { st(i16_type), st(i32_type), st(i64_type), st(i128_type) } }
				);
				ASSERT_EQUAL(st(expected), e_type);
			}
			{
				auto f_type   = getConstValueAs<compiler::tsh::SymbolType<>>("F", root_scope);
				auto expected = st(query::entryPoint<compiler::tsh::QueryTupleType>(
					{ { st(i16_type), st(i32_type), st(i64_type), st(i128_type) } }
				));
				ASSERT_EQUAL(expected, f_type);
			}
			{
				auto mega_type
					= getConstValueAs<compiler::tsh::SymbolType<>>("megaGigaType", root_scope);

				auto first  = st(unit_type);
				auto second = st(i128_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);
				auto third  = st(i32_type);
				auto fourth
					= st(query::entryPoint<compiler::tsh::QueryTupleType>({ { st(i16_type),
				                                                              st(f16_type) } }));
				auto fifth_inner_tuple
					= st(query::entryPoint<compiler::tsh::QueryTupleType>({ { st(f64_type),
				                                                              st(f128_type) } }));
				auto fifth = st(query::entryPoint<compiler::tsh::QueryVariantType>(
					{ { st(i64_type), fifth_inner_tuple } }
				));

				auto expected = st(query::entryPoint<compiler::tsh::QueryTupleType>(
					{ { first, second, third, fourth, fifth } }
				));

				ASSERT_EQUAL(expected, mega_type);
			}
			{
				auto first_type = getConstValueAs<compiler::tsh::SymbolType<>>("FIRST", root_scope);
				auto expected   = st(i16_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);
				ASSERT_EQUAL(expected, first_type);
			}
			{
				auto second_type
					= getConstValueAs<compiler::tsh::SymbolType<>>("SECOND", root_scope);
				auto expected = st(i64_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
				ASSERT_EQUAL(expected, second_type);
			}
			{  // Type Comparisons
				auto real_type = getConstValueAs<bool>("REAL", root_scope);
				ASSERT_EQUAL(real_type, true);
				auto fake_type = getConstValueAs<bool>("FAKE", root_scope);
				ASSERT_EQUAL(fake_type, false);
				auto mega_type = getConstValueAs<bool>("IS_MEGA", root_scope);
				ASSERT_EQUAL(mega_type, true);
				auto not_mega_type = getConstValueAs<bool>("NOT_IS_MEGA", root_scope);
				ASSERT_EQUAL(not_mega_type, false);
			}
		}
	}

	void testNumericLiterals() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/numeric_literals")));

		// General literal handling.
		for (auto val_name = 'A'; val_name <= 'G'; val_name++)
			ASSERT_EQUAL(10.125, getConstValueAs<f32>(std::string(1, val_name), root_scope));

		// Complex literals.
		ASSERT_EQUAL(5., getConstValueAs<f32>("tr_dot", root_scope));
		ASSERT_EQUAL(.5, getConstValueAs<f32>("lead_dot", root_scope));
		ASSERT_EQUAL(0.125, getConstValueAs<f32>("tr_e_dot", root_scope));
		ASSERT_EQUAL(2., getConstValueAs<f32>("lead_e_dot", root_scope));
		ASSERT_EQUAL(0.125, getConstValueAs<f32>("tr_big_e_dot", root_scope));
		ASSERT_EQUAL(2., getConstValueAs<f32>("lead_big_e_dot", root_scope));
		ASSERT_EQUAL(-10.125, getConstValueAs<f32>("neg", root_scope));

		// Test minimization logic.
		ASSERT_EQUAL(32'767, getConstValueAs<i32>("NEEDS_I32", root_scope));
		ASSERT_EQUAL(21'474'836'412, getConstValueAs<i64>("NEEDS_I64", root_scope));
		ASSERT_EQUAL(1.0f + 1.0f / 2048.0f, getConstValueAs<f32>("NEEDS_F32", root_scope));
		ASSERT_EQUAL(1.0 + 1.0 / 16777216.0, getConstValueAs<f64>("NEEDS_F64", root_scope));

		ASSERT_EQUAL(26, getConstValueAs<i16>("hex", root_scope));
		ASSERT_EQUAL(15, getConstValueAs<i16>("oct", root_scope));
		ASSERT_EQUAL(21, getConstValueAs<i16>("bin", root_scope));

		ASSERT_EQUAL(21, getConstValueAs<i32>("bin2", root_scope));

		// Test type deduction.
		const auto i16_type = getIntegralTypeNoContext(16, Signed);
		const auto i32_type = getIntegralTypeNoContext(32, Signed);
		const auto i64_type = getIntegralTypeNoContext(64, Signed);

		const auto u16_type = getIntegralTypeNoContext(16, Unsigned);
		const auto u32_type = getIntegralTypeNoContext(32, Unsigned);
		const auto u64_type = getIntegralTypeNoContext(64, Unsigned);

		const auto f32_type = getFloatTypeNoContext(32);
		const auto f64_type = getFloatTypeNoContext(64);


		auto verify_type_and_mutability = [&](std::string_view            keyword,
		                                      std::string_view            type_suffix,
		                                      compiler::tsh::AbstractType expected_type,
		                                      compiler::tsh::Mutability   expected_mutability) {
			auto var_name    = base::strConcat(keyword, "_", type_suffix);
			auto symbol_type = getSymbolTypeOf(var_name, root_scope);
			ASSERT_EQUAL(expected_type, symbol_type.getType());
			ASSERT_EQUAL(expected_mutability, symbol_type.getMutability());
		};

		verify_type_and_mutability("const", "i16", i16_type, Immutable);
		verify_type_and_mutability("const", "i32", i32_type, Immutable);
		verify_type_and_mutability("const", "i64", i64_type, Immutable);
		verify_type_and_mutability("const", "u16", u16_type, Immutable);
		verify_type_and_mutability("const", "u32", u32_type, Immutable);
		verify_type_and_mutability("const", "u64", u64_type, Immutable);
		verify_type_and_mutability("const", "f32", f32_type, Immutable);
		verify_type_and_mutability("const", "f64", f64_type, Immutable);

		verify_type_and_mutability("let", "i16", i16_type, Immutable);
		verify_type_and_mutability("let", "i32", i32_type, Immutable);
		verify_type_and_mutability("let", "i64", i64_type, Immutable);
		verify_type_and_mutability("let", "u16", u16_type, Immutable);
		verify_type_and_mutability("let", "u32", u32_type, Immutable);
		verify_type_and_mutability("let", "u64", u64_type, Immutable);
		verify_type_and_mutability("let", "f32", f32_type, Immutable);
		verify_type_and_mutability("let", "f64", f64_type, Immutable);

		verify_type_and_mutability("var", "i16", i16_type, Mutable);
		verify_type_and_mutability("var", "i32", i32_type, Mutable);
		verify_type_and_mutability("var", "i64", i64_type, Mutable);
		verify_type_and_mutability("var", "u16", u16_type, Mutable);
		verify_type_and_mutability("var", "u32", u32_type, Mutable);
		verify_type_and_mutability("var", "u64", u64_type, Mutable);
		verify_type_and_mutability("var", "f32", f32_type, Mutable);
		verify_type_and_mutability("var", "f64", f64_type, Mutable);
	}

	void testClassSymbolData() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes")));

		const auto first_class = getChain("FirstClassEver", root_scope).back();
		const auto first_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(first_class)->valueOrThrow();
		const auto first_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_class)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();

		ASSERT_EQUAL(2, first_class_info.members.size());
		ASSERT_EQUAL(2, first_class_info.methods.size());
		ASSERT_EQUAL(1, first_class_info.constructors.size());
		ASSERT_TRUE(first_class_info.destructor.has_value());
		ASSERT_TRUE(not first_class_info.base.has_value());
		ASSERT_EQUAL(0, first_class_info.implements.size());
		ASSERT_EQUAL("FirstClassEver", first_class_info.name);

		const auto& first_ctor
			= query::entryPoint<compiler::helios::defgen::QueryImplicitClassConstructor>(
				  first_class_abstract_type
			)
		          ->valueOrPanic();
		ASSERT_EQUAL(first_ctor.declaration->return_type.getType(), first_class_abstract_type);

		const auto second_class = getChain("SecondClass", root_scope).back();
		auto       second_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(second_class)->valueOrThrow();

		ASSERT_EQUAL(0, second_class_info.members.size());
		ASSERT_EQUAL(0, second_class_info.methods.size());
		ASSERT_EQUAL(0, second_class_info.constructors.size());
		ASSERT_TRUE(not second_class_info.destructor.has_value());
		ASSERT_TRUE(second_class_info.base.has_value());
		ASSERT_EQUAL(first_class_abstract_type, second_class_info.base);
		ASSERT_EQUAL("SecondClass", second_class_info.name);

		const auto class_with_member = getChain("ClassWithMember", root_scope).back();
		auto       class_with_member_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(class_with_member)
		          ->valueOrThrow();
		auto class_with_member_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(class_with_member)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();

		ASSERT_EQUAL(1, class_with_member_info.members.size());
		ASSERT_EQUAL(1, class_with_member_info.methods.size());

		const auto& class_with_members_ctor
			= query::entryPoint<compiler::helios::defgen::QueryImplicitClassConstructor>(
				  class_with_member_abstract_type
			)
		          ->valueOrPanic();
		ASSERT_EQUAL(
			class_with_members_ctor.declaration->return_type.getType(),
			class_with_member_abstract_type
		);

		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module_id })
		          .valueOrPanic();
		(void) units;  // @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		               // that it is successful
	}

	void testClassInteractions() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes_3")));

		const auto class_with_member = getChain("ClassWithMember", root_scope).back();
		const auto class_with_member_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(class_with_member)
		          ->valueOrThrow()
		          .getType();

		const auto first_class = getChain("FirstClass", root_scope).back();
		const auto first_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_class)
		          ->valueOrThrow()
		          .getType();

		auto c_symbol = getChain("c", root_scope).back();
		auto c_type
			= query::entryPoint<compiler::helios::QueryTypeOfSymbol>(c_symbol)->valueOrThrow();
		ASSERT_EQUAL(c_type, st(class_with_member_abstract_type));

		auto c_member_symbol = getChain("c_member", root_scope).back();
		auto c_member_type = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(c_member_symbol)
		                         ->valueOrThrow();

		ASSERT_EQUAL(c_member_type, st(first_class_abstract_type));

		auto c_member_a_symbol = getChain("c_member_a", root_scope).back();
		auto c_member_a_type
			= query::entryPoint<compiler::helios::QueryTypeOfSymbol>(c_member_a_symbol)
		          ->valueOrThrow();
		auto expected_type = st(getIntegralTypeNoContext(64, Signed));
		ASSERT_EQUAL(c_member_a_type, expected_type);

		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module_id })
		          .valueOrPanic();
		(void) units;  // @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		               // that it is successful
	}

	/**
	 * Simple checks that type instance interfaces return expected results.
	 * @note For now only checks interfaces of class types.
	 */
	void testTypeInstanceInterface() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/classes_2")));

		const auto simple_class = getChain("SimpleClass", root_scope).back();
		const auto simple_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(simple_class)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();

		const auto h_interface
			= compiler::helios::HInterface::ofTypeInstance(simple_class_abstract_type);

		query::utils::withContextDo([&](query::Context& ctx) {
			using compiler::helios::LookupResult;
			CRef<LookupResult> a_result
				= &h_interface.lookup(ctx, base::StrID("a"))->valueOrPanic();
			ASSERT_TRUE(a_result->isSingle());
			auto a_symbol = a_result->leaves.at(0);

			ASSERT_EQUAL(kind(a_symbol), compiler::helios::SymbolKind::Field);

			CRef<LookupResult> get_a_result
				= &h_interface.lookup(ctx, base::StrID("getA"))->valueOrPanic();
			ASSERT_TRUE(get_a_result->isSingle());
			auto get_a_symbol = get_a_result->leaves.at(0);
			ASSERT_EQUAL(kind(get_a_symbol), compiler::helios::SymbolKind::Method);

			CRef<LookupResult> empty_result
				= &h_interface.lookup(ctx, base::StrID("non_existent_symbol"))->valueOrPanic();
			ASSERT_TRUE(empty_result->isEmpty());
		});
	}

	void testTypeOf() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/types")));

		const auto int16_type = getIntegralTypeNoContext(16, Signed);
		const auto int32_type = getIntegralTypeNoContext(32, Signed);
		const auto f16_type   = getFloatTypeNoContext(16);
		const auto f32_type   = getFloatTypeNoContext(32);
		const auto bool_type  = compiler::tsh::getBoolType();
		const auto meta_type  = compiler::tsh::getMetaType();
		const auto str_type   = compiler::tsh::getStringType();

		const auto int32_mut_symbol_type   = st(int32_type).withMutability(Mutable);
		const auto int32_immut_symbol_type = st(int32_type).withMutability(Immutable);

		ASSERT_EQUAL(int32_immut_symbol_type, getSymbolTypeOf("SimpleIntConst", root_scope));
		ASSERT_EQUAL(int32_immut_symbol_type, getSymbolTypeOf("SimpleIntLet", root_scope));
		ASSERT_EQUAL(int32_mut_symbol_type, getSymbolTypeOf("SimpleIntVar", root_scope));

		ASSERT_EQUAL(f32_type, getTypeOf("SimpleFloat", root_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("SimpleBool", root_scope));
		ASSERT_EQUAL(str_type, getTypeOf("SimpleString", root_scope));

		const auto tuple_int_int               = getTypeOf("TupleII", root_scope);
		const auto tuple_int_int_abstract_type = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { st(int32_type), st(int32_type) } }
		);
		ASSERT_EQUAL(tuple_int_int, tuple_int_int_abstract_type);

		const auto first_variant               = getTypeOf("first_variant", root_scope);
		const auto first_variant_abstract_type = query::entryPoint<compiler::tsh::QueryVariantType>(
			{ { st(int32_type), st(f32_type) } }
		);
		ASSERT_EQUAL(first_variant, first_variant_abstract_type);

		const auto second_variant = getTypeOf("second_variant", root_scope);
		const auto second_variant_abstract_type
			= query::entryPoint<compiler::tsh::QueryVariantType>(
				{ { st(int32_type), st(f32_type), st(bool_type) } }
			);
		ASSERT_EQUAL(second_variant, second_variant_abstract_type);

		const auto weird_variant = getTypeOf("weird_variant", root_scope);

		const auto class_a = getTypeFromDefinition("A", root_scope);
		const auto class_b = getTypeFromDefinition("B", root_scope);
		const auto class_c = getTypeFromDefinition("C", root_scope);

		auto right_tuple = query::entryPoint<compiler::tsh::QueryTupleType>({ {
			class_a,
			st(query::entryPoint<compiler::tsh::QueryVariantType>({ { class_b, class_c } })),
		} });

		const auto weird_variant_type
			= query::entryPoint<compiler::tsh::QueryVariantType>({ { class_a, st(right_tuple) } });

		ASSERT_EQUAL(weird_variant, weird_variant_type);

		ASSERT_EQUAL(meta_type, getTypeOf("T", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("A", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("B", root_scope));
		ASSERT_EQUAL(meta_type, getTypeOf("C", root_scope));

		const auto tuple_ii_ff = getTypeOf("TupleIIFF", root_scope);

		const auto tuple_f16_f32
			= query::entryPoint<compiler::tsh::QueryTupleType>({ { st(f16_type), st(f32_type) } });
		const auto tuple_i16_i32 = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { st(int16_type), st(int32_type) } }
		);

		const auto tuple_ii_ff_abstract_type = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { st(tuple_i16_i32), st(tuple_f16_f32) } }
		);

		ASSERT_EQUAL(tuple_ii_ff, tuple_ii_ff_abstract_type);
	}

	void testEdgeEvals() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/edge_evals")));
		ASSERT_EQUAL(1, getConstValueAs<i32>("M1", root_scope));
		ASSERT_EQUAL(6, getConstValueAs<i32>("M2", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("O1", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i64>("O2", root_scope));
	}

	/**
	 * Test clone functionality by creating one big nested expression
	 * that contains every expression type at least once
	 */
	void testExprClone() {
		using namespace compiler::helios::code;

		const auto [_, root_scope] = getModule(fs::File(path("test_modules/expressions")));
		const auto [func_module, func_scope]
			= getModule(fs::File(path("test_modules/function_calls")));
		const auto a_obj      = getChain("aObj", root_scope).back();
		const auto square_sym = getChain("square", func_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto int_type = compiler::tsh::getIntegralType(ctx, 64, Signed);


			// Build chain comparison expressions vector (1 < 2 <= 3)
			auto first_expr = makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 1);
			auto second_expr
				= makeBox<ReusableExpr>(ctx, makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 2));
			auto second_expr_reused = second_expr->nextUse();
			auto third_expr         = makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 3);

			// Build the comparisons vector
			std::vector<Box<Expr>> comparisons;
			comparisons.emplace_back(makeBox<BinaryOperatorExpr>(
				ctx,
				generatedOrigin(),
				BuiltinBinary::IntegerLt,
				std::move(first_expr),
				std::move(second_expr)
			));
			comparisons.emplace_back(makeBox<BinaryOperatorExpr>(
				ctx,
				generatedOrigin(),
				BuiltinBinary::IntegerLteq,
				std::move(second_expr_reused),
				std::move(third_expr)
			));

			// Build tuple elements
			std::vector<base::Box<Expr>> tuple_elements;
			tuple_elements.emplace_back(makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 1));
			tuple_elements.emplace_back(makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 2));

			// Build call arguments for square function
			const compiler::helios::SymID a_field
				= ctx.query<compiler::helios::QueryTypeOfSymbol>(a_obj)
			          ->valueOrThrow()
			          .getType()
			          .getInterface(ctx)
			          ->getElementsWithName(base::StrID("a"))
			          .back()
			          .getSymbol();
			std::vector<base::Box<Expr>> call_args;
			call_args.emplace_back(makeBox<AccessExpr>(
				ctx, generatedOrigin(), makeBox<IdentifierExpr>(ctx, generatedOrigin(), a_obj), a_field
			));

			// Build sequence expressions
			std::vector<base::Box<Expr>> sequence_exprs;
			sequence_exprs.emplace_back(
				makeBox<TupleExpr>(ctx, generatedOrigin(), std::move(tuple_elements))
			);
			sequence_exprs.emplace_back(makeBox<BinaryOperatorExpr>(
				ctx,
				generatedOrigin(),
				BuiltinBinary::IntegerAdd,
				makeBox<ParenthesisExpr>(
					ctx,
					generatedOrigin(),
					makeBox<UnaryOperatorExpr>(
						ctx,
						generatedOrigin(),
						BuiltinUnary::IntegerNegation,
						makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 10)
					)
				),
				makeBox<CallExpr>(
					ctx,
					generatedOrigin(),
					makeBox<IdentifierExpr>(ctx, generatedOrigin(), square_sym),
					std::move(call_args)
				)
			));

			// Build variant subtypes
			std::vector<base::Box<Expr>> variant_subtypes;
			variant_subtypes.emplace_back(makeBox<LiteralTypeExpr>(ctx, generatedOrigin(), int_type)
			);
			variant_subtypes.emplace_back(makeBox<LiteralBoolExpr>(ctx, generatedOrigin(), true));
			variant_subtypes.emplace_back(
				makeBox<LiteralStringExpr>(ctx, generatedOrigin(), base::StrID("hello"))
			);

			auto mega_expr = makeBox<TernaryOperatorExpr>(
				ctx,
				generatedOrigin(),
				// Condition: ChainComparisonExpr (1 < 2 <= 3)
				makeBox<ChainComparisonExpr>(ctx, generatedOrigin(), std::move(comparisons)),
				// If true: SequenceExpr with nested expressions including CallExpr
				makeBox<SequenceExpr>(ctx, generatedOrigin(), std::move(sequence_exprs)),
				// If false: VariantTypeConstructorExpr(i64 | bool | string)
				makeBox<VariantTypeConstructorExpr>(
					ctx, generatedOrigin(), std::move(variant_subtypes)
				)
			);

			// Test the clone
			auto cloned = mega_expr->clone();

			std::stringstream orig_out, clone_out;
			mega_expr->debugPrint(orig_out);
			cloned->debugPrint(clone_out);

			// Verify they produce same debug output
			ASSERT_EQUAL(orig_out.str(), clone_out.str());
			// Verify they are different objects
			assertTrue(&(*mega_expr) != &(*cloned), "Clone should be a different object");
		});
	}

	void testSimpleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_simple_test")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		ASSERT_EQUAL(hout.functions.size(), 3);
		ASSERT_EQUAL(hout.glob_data.size(), 3);

		// just for cov and to see if it does not throw:
		query::utils::withContextDo([&](query::Context& ctx) {
			std::stringstream ss;
			hout.debugPrint(ctx, ss);
		});
	}

	void testSingleFileModuleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/simple_scopes")));

		auto houts
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module).valueOrPanic();

		unsigned long functions = 0;
		unsigned long glob_data = 0;

		for (const auto& hout: houts) {
			functions += hout->functions.size();
			glob_data += hout->glob_data.size();
		}

		ASSERT_EQUAL(functions, 3);
		ASSERT_EQUAL(glob_data, 5);
	}

	void testModuleHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_module")));

		auto houts
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module).valueOrPanic();

		unsigned long functions = 0;
		unsigned long glob_data = 0;

		for (const auto& hout: houts) {
			functions += hout->functions.size();
			glob_data += hout->glob_data.size();
		}

		ASSERT_EQUAL(functions, 1);
		ASSERT_EQUAL(glob_data, 5);
	}

	void testDependencyHOUT() {
		auto [module, _] = getModule(fs::File(path("test_modules/hout_simple_test")));

		auto houts
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module).valueOrPanic();

		for (const auto& hout: houts) {
			for (const auto& fun: hout->functions) {
				std::cerr << fun->declaration->original_name.str() << " is dependent on\n";
				std::cerr << " Original symbol: "
						  << fun->declaration->original_symbol.queryUnstablePerfectHash() << "\n";
				auto positions = pst::queryPositionDependencies<compiler::helios::QueryCodeOfFun>(
					fun->declaration->original_symbol
				);
				std::cerr << "Tokens:\n";

				auto tokens = pst::queryTokenDependencies<compiler::helios::QueryCodeOfFun>(
					fun->declaration->original_symbol
				);

				printer::PrinterOStream str;
				dia::printHighlightedPositions(str, positions);

				printer::StreamPrinter p;
				p.print(str.getContents());
			}
		}
	}

	void testHoutVisitor() {
		auto module = compiler::frontend::createModuleTreeWithRandomPackageID(
			fs::File(path("test_modules/visitor_test_module"))
		);

		const auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		ASSERT_EQUAL(hout.functions.size(), 1);

		auto the_function = hout.functions.at(0);

		auto& stmt_list = the_function->body->statements;
		ASSERT_EQUAL(stmt_list.size(), 6);

		using namespace compiler::helios::code;

		struct StmtVisitor: public HoutStmtVisitorPanicky {
			usize expr_stmt_count        = 0;
			usize return_stmt_count      = 0;
			usize void_return_stmt_count = 0;
			usize if_stmt_count          = 0;
			usize while_stmt_count       = 0;

			void visitExprStmt(const ExprStmt&) override { expr_stmt_count++; }

			void visitReturnStmt(const ReturnStmt&) override { return_stmt_count++; }

			void visitVoidReturnStmt(const VoidReturnStmt&) override { void_return_stmt_count++; }

			void visitIfStmt(const IfStmt&) override { if_stmt_count++; }

			void visitWhileStmt(const WhileStmt&) override { while_stmt_count++; }
		};

		{
			StmtVisitor visitor;
			stmt_list.at(0)->acceptVisitor(visitor);
			stmt_list.at(1)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_stmt_count, 2);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(2)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.return_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(3)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.void_return_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(4)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.if_stmt_count, 1);
		}
		{
			StmtVisitor visitor;
			stmt_list.at(5)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.while_stmt_count, 1);
		}

		struct ExprVisitor: public HoutExprVisitorPanicky {
			usize const_int_count = 0;
			usize ident_count     = 0;

			void visitLiteralNumericExpr(const LiteralNumericExpr&) override { const_int_count++; }

			void visitIdentifierExpr(const IdentifierExpr&) override { ident_count++; }

			void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override { ident_count++; }

			void visitParenthesisExpr(const ParenthesisExpr&) override { ident_count++; }
		};

		struct ExprVisitorRunner: public HoutStmtVisitorPanicky {
			ExprVisitor expr_visitor;

			void visitExprStmt(const ExprStmt& expr) override {
				expr.expr->acceptVisitor(expr_visitor);
			}
		};

		{
			ExprVisitorRunner visitor;
			stmt_list.at(0)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_visitor.const_int_count, 1);
		}
		{
			ExprVisitorRunner visitor;
			stmt_list.at(1)->acceptVisitor(visitor);
			ASSERT_EQUAL(visitor.expr_visitor.ident_count, 1);
		}
	}

	void testImport() {
		auto [module, _] = getModule(fs::File(path("test_modules/import_tests")));

		(void) query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module);

		const auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto test_value = [&](auto str, i64 exp_val) {
			auto name = base::StrID(str);
			for (auto& gb: hout.glob_data) {
				if (gb->original_name == name) {
					if (std::holds_alternative<compiler::helios::HOUTGlobalConst>(gb->value)) {
						auto ctv = std::get<compiler::helios::HOUTGlobalConst>(gb->value).value;
						auto val = ctv.get<compiler::numeric_value::NumericValue>()->get<i64>();
						if (!val.has_value()) {
							this->fail(base::strConcat(
								"Got a constant with a different type than expected: ",
								name.strView()
							));
						}
						ASSERT_EQUAL(exp_val, val);
						return;
					} else {
						this->fail(base::strConcat("Expected constant but found: ", name.strView()));
					}
				}
			}
			this->fail(base::strConcat("No constant of name: ", name.strView()));
		};

		test_value("sm1_v", 123'123);
		test_value("sm11_v", 7'812'313);
		test_value("it_through_alias", 19'923);
		test_value("sm1_through_sm11", 123'123);
		test_value("sm2_v", 777'666);
		test_value("cyclic_final", 6);
	}

	void testExprTree() {
		auto symbol_name = [](const char* name, auto&& symbol) {
			return base::strConcat("(Symbol ", name, " (", symbol.queryUnstablePerfectHash(), "))");
		};
		auto tmp   = [](const std::string& expr) { return base::strConcat("[tmp](", expr, ")"); };
		auto reuse = [](const std::string& expr) { return base::strConcat("[reuse](", expr, ")"); };

		auto [_, root_scope] = getModule(fs::File(path("test_modules/expressions")));

		ASSERT_EQUAL(1, getConstValueAs<i32>("V1", root_scope));
		auto              sym_v1  = getChain("V1", root_scope).back();
		auto              tree_v1 = getExprOfConst(sym_v1);
		std::stringstream out_v1;
		tree_v1->debugPrint(out_v1);

		ASSERT_EQUAL(-1, getConstValueAs<i32>("VM1", root_scope));
		auto              sym_vm1  = getChain("VM1", root_scope).back();
		auto              tree_vm1 = getExprOfConst(sym_vm1);
		std::stringstream out_vm1;
		tree_vm1->debugPrint(out_vm1);

		ASSERT_EQUAL(256, getConstValueAs<i32>("V256", root_scope));

		auto              sym_v256 = getChain("V256", root_scope).back();
		std::stringstream out_v256;
		auto              tree_v256 = getExprOfConst(sym_v256);
		tree_v256->debugPrint(out_v256);
		ASSERT_EQUAL("(3 + 4 - 4 * 16 / 5 % 7) ** 8", out_v256.str());

		ASSERT_EQUAL(12, getConstValueAs<i64>("V12", root_scope));
		auto              sym_v12  = getChain("V12", root_scope).back();
		auto              tree_v12 = getExprOfConst(sym_v12);
		std::stringstream out_v12;
		tree_v12->debugPrint(out_v12);

		auto sym_v3      = getChain("N.V3", root_scope).back();
		auto sym_v3_repr = symbol_name("V3", sym_v3);
		ASSERT_EQUAL(
			base::strConcat(sym_v3_repr, " + ", sym_v3_repr, " * ", sym_v3_repr), out_v12.str()
		);

		ASSERT_EQUAL(false, getConstValueAs<bool>("CMP", root_scope));
		auto              get_cmp  = getChain("CMP", root_scope).back();
		auto              expr_cmp = getExprOfConst(get_cmp);
		std::stringstream out_cmp;
		expr_cmp->debugPrint(out_cmp);
		ASSERT_EQUAL_PRINT(
			(base::strConcat(
				symbol_name("V1", sym_v1),
				" < ",
				tmp("3"),
				" and ",
				reuse("3"),
				" <= ",
				tmp("4"),
				" and ",
				reuse("4"),
				" == ",
				tmp("5"),
				" and ",
				reuse("5"),
				" != ",
				tmp("6"),
				" and ",
				reuse("6"),
				" >= ",
				tmp("7"),
				" and ",
				reuse("7"),
				" > ",
				tmp(symbol_name("VM1", sym_vm1))
			)),
			out_cmp.str()
		);

		auto get_str  = getChain("STR", root_scope).back();
		auto expr_str = getExprOfConst(get_str);
		Ref  expr_str_casted
			= dynamic_cast<const compiler::helios::code::LiteralStringExpr*>(&*expr_str);
		ASSERT_EQUAL("quack", expr_str_casted->value.str());

		auto              sym_vref  = getChain("VREF", root_scope).back();
		auto              tree_vref = getExprOfConst(sym_vref);
		std::stringstream out_vref;
		tree_vref->debugPrint(out_vref);
		const auto int32_type    = getIntegralTypeNoContext(32, Signed);
		const auto int32ref_type = st(int32_type)
		                               .withReferenceKind(compiler::tsh::ReferenceKind::Ref)
		                               .withMutability(Immutable);
		const auto vref_type = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vref);
		ASSERT_EQUAL(int32ref_type, vref_type->valueOrThrow());

		auto              sym_vbox  = getChain("VBOX", root_scope).back();
		auto              tree_vbox = getExprOfConst(sym_vbox);
		std::stringstream out_vbox;
		tree_vbox->debugPrint(out_vbox);
		const auto f16_type    = getFloatTypeNoContext(16);
		const auto f16box_type = st(f16_type)
		                             .withReferenceKind(compiler::tsh::ReferenceKind::Box)
		                             .withMutability(Immutable);
		const auto vbox_type = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vbox);
		ASSERT_EQUAL(f16box_type, vbox_type->valueOrThrow());

		auto              sym_vconst  = getChain("VCONST", root_scope).back();
		auto              tree_vconst = getExprOfConst(sym_vconst);
		std::stringstream out_vconst;
		tree_vconst->debugPrint(out_vconst);
		const auto bool_type       = compiler::tsh::getBoolType();
		const auto const_bool_type = st(bool_type).withMutability(Immutable);
		const auto vconst_type = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vconst);
		ASSERT_EQUAL(const_bool_type, vconst_type->valueOrThrow());

		auto member_access_sym  = getChain("member_access", root_scope).back();
		auto member_access_expr = getExprOfVariable(member_access_sym);
		ASSERT_EQUAL(
			member_access_expr->expression_type.getType(), getIntegralTypeNoContext(32, Signed)
		);
	}

	void testHoutVariables() {
		auto [module, _] = getModule(fs::File(path("test_modules/variables")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		ASSERT_EQUAL(hout.functions.size(), 1);

		auto& function = hout.functions.at(0);

		ASSERT_EQUAL(function->declaration->original_name, "foo");

		// note that alias should not be included here:
		ASSERT_EQUAL(function->body->statements.size(), 8);

		auto& statements = function->body->statements;

		auto get_var_block = [&](usize i, auto&& code_block) -> decltype(auto) {
			return dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*code_block.statements.at(i)
			);
		};

		auto get_var_ref
			= [&](usize i) -> decltype(auto) { return get_var_block(i, *function->body); };


		auto i32_type = getIntegralTypeNoContext(32, Signed);
		auto f32_type = getFloatTypeNoContext(32);
		auto i32_or_f32
			= query::entryPoint<compiler::tsh::QueryVariantType>({ { st(i32_type), st(f32_type) } });


		{
			auto& var = get_var_ref(0);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "a");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			auto& var = get_var_ref(1);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "b");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			// @TODO: #803 support variant types
			// auto& var = get_var_ref(2);
			// ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "c");
			// ASSERT_EQUAL(var.type, st(i32_or_f32));
			(void) i32_or_f32;  // < remove
		}

		{
			auto& var = get_var_ref(2);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "d");
			ASSERT_EQUAL(var.type.getType().getKind(), compiler::tsh::Kind::Class);
		}

		{
			auto& if_stmt = dynamic_cast<const compiler::helios::code::IfStmt&>(*statements.at(3));
			{
				auto& var1 = get_var_block(0, if_stmt.then_body);
				ASSERT_EQUAL(compiler::helios::name(var1.helios_symbol), "x");
				ASSERT_EQUAL(var1.type, st(i32_type));

				// @TODO: #803 support variant types
				// auto& var2 = get_var_block(1, if_stmt.then_body);
				// ASSERT_EQUAL(compiler::helios::name(var2.helios_symbol), "y");
				// ASSERT_EQUAL(var2.type, st(i32_or_f32));
			}
			{
				auto& var = get_var_block(0, if_stmt.else_body);
				ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "z");
				ASSERT_EQUAL(var.type, st(i32_type));
			}
		}

		{
			auto& while_stmt
				= dynamic_cast<const compiler::helios::code::WhileStmt&>(*statements.at(4));
			auto& var = get_var_block(0, while_stmt.body);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "a");
			ASSERT_EQUAL(var.type, st(i32_type));
		}

		{
			// @TODO: #1412 fix dealias
			// auto& var = get_var_ref(6);
			// ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "e");
			// ASSERT_EQUAL(var.type.getType().getKind(), compiler::tsh::Kind::Class);
		}

		// debug print test just for cov and to see if it does not throw:
		query::utils::withContextDo([&](query::Context& ctx) {
			std::stringstream ss;
			hout.debugPrint(ctx, ss);
		});
	}

	void testReferences() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/references")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(0);

		{
			// Check type of r.
			auto test_simple_ref       = getChain("test_simple_ref", top_scope).back();
			auto test_simple_ref_scope = getFunctionBodyScope(test_simple_ref);
			auto i32_type              = getIntegralTypeNoContext(32, Signed);
			auto expected_type = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
			ASSERT_EQUAL(expected_type, getSymbolTypeOf("r", test_simple_ref_scope));
		}
		{
			// Check if RefOfExpr was inserted.
			auto& var_stmt = dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*function->body->statements.at(1)
			);
			auto make_ref_expr = dynamic_cast<const compiler::helios::code::RefOfExpr*>(
				var_stmt.initial_value.get()
			);
			ASSERT_TRUE(make_ref_expr != nullptr);
		}
		{
			// Check if deref was inserted when assigning a `ref T = T`
			auto& ass_stmt = dynamic_cast<const compiler::helios::code::AssignmentStmt&>(
				*function->body->statements.at(2)
			);
			auto deref_expr = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				ass_stmt.location_expr.get()
			);
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// Check if deref was inserted when assigning T = ref T.
			auto& var_stmt = dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*function->body->statements.at(3)
			);
			auto deref_expr = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				var_stmt.initial_value.get()
			);
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// Check if `ref T = ref T` performs value assignment, not rebinding.
			auto& ass_stmt = dynamic_cast<const compiler::helios::code::AssignmentStmt&>(
				*function->body->statements.at(5)
			);
			auto deref_lhs = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				ass_stmt.location_expr.get()
			);
			auto deref_rhs = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				ass_stmt.new_value_expr.get()
			);
			ASSERT_TRUE(deref_lhs != nullptr);
			ASSERT_TRUE(deref_rhs != nullptr);
		}
		{
			// Check if ref T = ref T + 1. Derefs should be inserted on both sides.
			auto& ass_stmt = dynamic_cast<const compiler::helios::code::AssignmentStmt&>(
				*function->body->statements.at(6)
			);

			auto deref1_expr = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				ass_stmt.location_expr.get()
			);
			auto bin_expr = dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(
				ass_stmt.new_value_expr.get()
			);
			auto deref2_expr
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(bin_expr->lhs.get());
			ASSERT_TRUE(deref1_expr != nullptr);
			ASSERT_TRUE(deref2_expr != nullptr);
		}
		{
			// Check deref in call expressions.
			auto& call_stmt = dynamic_cast<const compiler::helios::code::ExprStmt&>(
				*function->body->statements.at(7)
			);
			auto& call_expr
				= dynamic_cast<const compiler::helios::code::CallExpr&>(*call_stmt.expr.get());

			auto deref_expr = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				call_expr.arguments.at(0).get()
			);
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// Check deref in unary operator: var z: i32 = -r;
			auto& var_stmt = dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*function->body->statements.at(8)
			);
			auto un_expr = dynamic_cast<const compiler::helios::code::UnaryOperatorExpr*>(
				var_stmt.initial_value.get()
			);
			ASSERT_TRUE(un_expr != nullptr);

			// The operand of '-' should be a DerefExpr
			auto deref_expr
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(un_expr->expr.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// Check deref in binary operator with two refs: var p: i32 = r + r2;
			auto& var_stmt = dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*function->body->statements.at(9)
			);
			auto bin_expr = dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(
				var_stmt.initial_value.get()
			);
			ASSERT_TRUE(bin_expr != nullptr);

			// Both sides of '+' should be DerefExpr
			auto deref_lhs
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(bin_expr->lhs.get());
			auto deref_rhs
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(bin_expr->rhs.get());

			ASSERT_TRUE(deref_lhs != nullptr);
			ASSERT_TRUE(deref_rhs != nullptr);
		}
		{
			// Check deref in field access: var val: i32 = ref_point.x;
			auto& var_stmt = dynamic_cast<const compiler::helios::code::VariableStmt&>(
				*function->body->statements.at(12)
			);
			auto outer_deref = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				var_stmt.initial_value.get()
			);
			ASSERT_TRUE(outer_deref != nullptr);
			auto access_expr
				= dynamic_cast<const compiler::helios::code::AccessExpr*>(outer_deref->inner.get());
			ASSERT_TRUE(access_expr != nullptr);

			auto inner_deref
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(access_expr->base.get());
			ASSERT_TRUE(inner_deref != nullptr);
			auto ident_expr = dynamic_cast<const compiler::helios::code::IdentifierExpr*>(
				inner_deref->inner.get()
			);
			ASSERT_TRUE(ident_expr != nullptr);
		}
		{
			// Check deref in returns.
			auto& ret_stmt = dynamic_cast<const compiler::helios::code::ReturnStmt&>(
				*function->body->statements.at(13)
			);
			auto deref_expr
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(ret_stmt.value.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
	}

	void testBoxes() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/boxes")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(4);
		auto& body     = *function->body;
		using namespace compiler::helios::code;

		{
			// var b_int: box i32 = 42;
			ASSERT_TRUE(body.statements.size() > 0);
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[0].get());
			ASSERT_TRUE(var_stmt != nullptr);

			auto* make_box_expr = dynamic_cast<const BoxOfExpr*>(var_stmt->initial_value.get());
			ASSERT_TRUE(make_box_expr != nullptr);

			auto* literal_expr
				= dynamic_cast<const LiteralNumericExpr*>(make_box_expr->inner.get());
			ASSERT_TRUE(literal_expr != nullptr);

			auto var_type = var_stmt->type;
			ASSERT_EQUAL(var_type.getRefKind(), compiler::tsh::ReferenceKind::Box);
			ASSERT_EQUAL(var_type.getType().getKind(), compiler::tsh::Kind::Integral);
		}
		{
			// double_coerce(b_int);
			// `box i32` -> `ref i32` -> `ref i64`
			ASSERT_TRUE(body.statements.size() > 2);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[1].get());
			ASSERT_TRUE(expr_stmt != nullptr);
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* cast_expr = dynamic_cast<const CastExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(cast_expr != nullptr);
			ASSERT_EQUAL(cast_expr->target_type.getRefKind(), compiler::tsh::ReferenceKind::Ref);
		}
		{
			// var x: i32 = b_point.x;
			ASSERT_TRUE(body.statements.size() > 5);
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[4].get());
			ASSERT_TRUE(var_stmt != nullptr);

			auto* access_expr = dynamic_cast<const AccessExpr*>(var_stmt->initial_value.get());
			ASSERT_TRUE(access_expr != nullptr);
			ASSERT_EQUAL(compiler::helios::name(access_expr->field), "x");

			auto* deref_expr = dynamic_cast<const DerefExpr*>(access_expr->base.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// b_point.y = 99;
			ASSERT_TRUE(body.statements.size() > 7);
			auto* assign_stmt = dynamic_cast<const AssignmentStmt*>(body.statements[6].get());
			ASSERT_TRUE(assign_stmt != nullptr);

			auto* access_expr = dynamic_cast<const AccessExpr*>(assign_stmt->location_expr.get());
			ASSERT_TRUE(access_expr != nullptr);
			ASSERT_EQUAL(compiler::helios::name(access_expr->field), "y");

			auto* deref_expr = dynamic_cast<const DerefExpr*>(access_expr->base.get());
			ASSERT_TRUE(deref_expr != nullptr);
		}
		{
			// by_val(b_point);
			// `box T -> T`
			ASSERT_TRUE(body.statements.size() > 8);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[7].get());
			ASSERT_TRUE(expr_stmt != nullptr);
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* deref_expr = dynamic_cast<const DerefExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(deref_expr != nullptr);

			auto* ident_expr = dynamic_cast<const IdentifierExpr*>(deref_expr->inner.get());
			ASSERT_TRUE(ident_expr != nullptr);
		}
		{
			// by_ref(b_point);
			// `box T -> ref T`
			ASSERT_TRUE(body.statements.size() > 9);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[8].get());
			ASSERT_TRUE(expr_stmt != nullptr);

			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* refof_expr = dynamic_cast<const RefOfExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(refof_expr != nullptr);

			auto* ident_expr = dynamic_cast<const IdentifierExpr*>(refof_expr->inner.get());
			ASSERT_TRUE(ident_expr != nullptr);
		}
		{
			// `box i32 -> i32` in return.
			auto* return_stmt = dynamic_cast<const ReturnStmt*>(body.statements.back().get());
			ASSERT_TRUE(return_stmt != nullptr);

			auto* deref_expr = dynamic_cast<const DerefExpr*>(return_stmt->value.get());
			ASSERT_TRUE(deref_expr != nullptr);

			auto* ident_expr = dynamic_cast<const IdentifierExpr*>(deref_expr->inner.get());
			ASSERT_TRUE(ident_expr != nullptr);
		}
	}

	void testReferenceKindCollapsing() {
		auto [module, top_scope]
			= getModule(fs::File(path("test_modules/reference_kind_collapsing")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(0);
		auto& body     = *function->body;
		using namespace compiler::helios::code;

		auto i32_type
			= getIntegralTypeNoContext(32, compiler::tsh::IntegralAbstractType::Signedness::Signed);
		auto ref_i32 = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Ref);
		auto box_i32 = st(i32_type).withReferenceKind(compiler::tsh::ReferenceKind::Box);

		auto get_var_stmt = [&](usize index) -> const VariableStmt& {
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[index].get());
			ASSERT_TRUE(var_stmt != nullptr);
			return *var_stmt;
		};

		// var ref_ref_a: ref i32 = &ref_a; (Ref -> Ref)
		{
			const auto& var_stmt = get_var_stmt(3);
			ASSERT_EQUAL(var_stmt.type, ref_i32);
			// `&ref_a` should just copy the pointer, which is a simple assignment.
			// The explicit `&` creates a RefOfExpr, and type system collapses the type.
			auto* ref_of = dynamic_cast<const RefOfExpr*>(var_stmt.initial_value.get());
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var ref_box_a: ref i32 = &box_a; (Box -> Ref)
		{
			const auto& var_stmt = get_var_stmt(4);
			ASSERT_EQUAL(var_stmt.type, ref_i32);
			auto* ref_of = dynamic_cast<const RefOfExpr*>(var_stmt.initial_value.get());
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var box_ref_a: box i32 = ref_a; (Ref -> Box)
		{
			const auto& var_stmt = get_var_stmt(5);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// This should create a copy. `BoxOfExpr(DerefExpr(...))`
			auto* box_of = dynamic_cast<const BoxOfExpr*>(var_stmt.initial_value.get());
			ASSERT_TRUE(box_of != nullptr);
			auto* deref = dynamic_cast<const DerefExpr*>(box_of->inner.get());
			ASSERT_TRUE(deref != nullptr);
		}
		// var box_box_a: box i32 = box_a; (Box -> Box)
		{
			const auto& var_stmt = get_var_stmt(6);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// This is just a move, should be a noop
			auto* ident = dynamic_cast<const IdentifierExpr*>(var_stmt.initial_value.get());
			ASSERT_TRUE(ident != nullptr);
		}
	}

	void testKeywordLiterals() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/keyword_literals")));

		auto i8_type   = getIntegralTypeNoContext(8, Signed);
		auto i16_type  = getIntegralTypeNoContext(16, Signed);
		auto i32_type  = getIntegralTypeNoContext(32, Signed);
		auto i64_type  = getIntegralTypeNoContext(64, Signed);
		auto i128_type = getIntegralTypeNoContext(128, Signed);

		auto u8_type   = getIntegralTypeNoContext(8, Unsigned);
		auto u16_type  = getIntegralTypeNoContext(16, Unsigned);
		auto u32_type  = getIntegralTypeNoContext(32, Unsigned);
		auto u64_type  = getIntegralTypeNoContext(64, Unsigned);
		auto u128_type = getIntegralTypeNoContext(128, Unsigned);

		auto f16_type = getFloatTypeNoContext(16);
		auto f32_type = getFloatTypeNoContext(32);
		auto f64_type = getFloatTypeNoContext(64);

		auto f80_type  = getFloatTypeNoContext(80);
		auto f128_type = getFloatTypeNoContext(128);

		auto char_type = compiler::tsh::getCharType();

		auto bool_type = compiler::tsh::getBoolType();

		auto str_type = compiler::tsh::getStringType();

		// a simple way to get function scope through hout:
		auto foo            = getChain("foo", top_scope).back();
		auto foo_body_scope = getFunctionBodyScope(foo);

		// variable types:

		ASSERT_EQUAL(i8_type, getTypeOf("v_i8", foo_body_scope));
		ASSERT_EQUAL(i16_type, getTypeOf("v_i16", foo_body_scope));
		ASSERT_EQUAL(i32_type, getTypeOf("v_i32", foo_body_scope));
		ASSERT_EQUAL(i64_type, getTypeOf("v_i64", foo_body_scope));
		ASSERT_EQUAL(i128_type, getTypeOf("v_i128", foo_body_scope));

		ASSERT_EQUAL(u8_type, getTypeOf("v_u8", foo_body_scope));
		ASSERT_EQUAL(u16_type, getTypeOf("v_u16", foo_body_scope));
		ASSERT_EQUAL(u32_type, getTypeOf("v_u32", foo_body_scope));
		ASSERT_EQUAL(u64_type, getTypeOf("v_u64", foo_body_scope));
		ASSERT_EQUAL(u128_type, getTypeOf("v_u128", foo_body_scope));

		ASSERT_EQUAL(f16_type, getTypeOf("v_f16", foo_body_scope));
		ASSERT_EQUAL(f32_type, getTypeOf("v_f32", foo_body_scope));
		ASSERT_EQUAL(f64_type, getTypeOf("v_f64", foo_body_scope));
		ASSERT_EQUAL(f80_type, getTypeOf("v_f80", foo_body_scope));
		ASSERT_EQUAL(f128_type, getTypeOf("v_f128", foo_body_scope));

		ASSERT_EQUAL(char_type, getTypeOf("v_char", foo_body_scope));

		ASSERT_EQUAL(bool_type, getTypeOf("v_bool_t", foo_body_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("v_bool_f", foo_body_scope));

		ASSERT_EQUAL(str_type, getTypeOf("v_str", foo_body_scope));

		// true, false literals:
		auto true_expr  = getExprOfVariable(getChain("v_bool_t", foo_body_scope).back());
		auto false_expr = getExprOfVariable(getChain("v_bool_f", foo_body_scope).back());

		auto true_expr_casted
			= dynamic_cast<const compiler::helios::code::LiteralBoolExpr*>(&*true_expr);
		auto false_expr_casted
			= dynamic_cast<const compiler::helios::code::LiteralBoolExpr*>(&*false_expr);

		ASSERT_TRUE(true_expr_casted != nullptr);
		ASSERT_TRUE(false_expr_casted != nullptr);

		ASSERT_EQUAL(true_expr_casted->value, true);
		ASSERT_EQUAL(false_expr_casted->value, false);
	}

	void testFunctionParameters() {
		auto [module, _] = getModule(fs::File(path("test_modules/parameters")));

		const auto int32_type = getIntegralTypeNoContext(32, Signed);
		const auto int64_type = getIntegralTypeNoContext(64, Signed);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto& hout = ctx.query<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
			ASSERT_EQUAL(hout.functions.size(), 2);
			{
				auto function = hout.functions.at(0);
				ASSERT_EQUAL(function->declaration->original_name, "foo");

				auto& a_param = function->declaration->parameters.at(0);
				ASSERT_EQUAL("a", a_param.name);
				ASSERT_EQUAL(st(int32_type), a_param.type);
				assertTrue(a_param.initial_value.empty(), "No initial value expected");

				// get "a" thru return:
				ASSERT_EQUAL(function->body->statements.size(), 1);

				auto ret_stmt = function->body->statements.at(0).ref();
				auto ret_stmt_casted
					= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
				assertTrue(ret_stmt_casted != nullptr, "Return statement expected");

				auto ret_expr = ret_stmt_casted->value.ref();
				auto ret_expr_casted
					= dynamic_cast<const compiler::helios::code::IdentifierExpr*>(&*ret_expr);
				assertTrue(ret_expr_casted != nullptr, "Identifier expression expected");

				auto a_sym  = ret_expr_casted->symbol;
				auto a_type = ret_expr_casted->expression_type;

				ASSERT_EQUAL(int32_type, a_type.getType());
				ASSERT_EQUAL(
					st(int32_type),
					ctx.query<compiler::helios::QueryTypeOfSymbol>({ a_sym })->valueOrThrow()
				);

				ASSERT_EQUAL(a_sym, a_param.helios_symbol);
			}

			{
				auto function = hout.functions.at(1);
				ASSERT_EQUAL(function->declaration->original_name, "bar");
				auto& abc_param    = function->declaration->parameters.at(0);
				auto& second_param = function->declaration->parameters.at(1);

				ASSERT_EQUAL("abc", abc_param.name);
				ASSERT_EQUAL("second", second_param.name);

				ASSERT_EQUAL(abc_param.type, st(int32_type));
				ASSERT_EQUAL(second_param.type, st(int64_type));

				assertTrue(abc_param.initial_value.has_value(), "Initial value expected");
				assertTrue(second_param.initial_value.empty(), "No initial value expected");
			}
		});
	}

	void testExprScopes() {
		auto [module, _] = getModule(fs::File(path("test_modules/expr_scopes")));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto main_file = ctx.query<compiler::frontend::QueryMainSourceFile>({ module });
			auto pst       = getFilePST(ctx, main_file);

			auto test_expr = [&](pst::AccessLocked<pst::ExprHolder> expr) {
				auto unlocked = expr.unlock(ctx);
				ASSERT_TRUE(unlocked->isTopLevel());
				auto expected_scope = ctx.query<compiler::helios::QueryPrimaryCodeScopeFor>(expr);

				// this can't be auto because of recursive lambda
				std::function<void(pst::AccessLocked<pst::LangElement>)> sub_test_expr
					= [&](pst::AccessLocked<pst::LangElement> inner_expr) {
						  auto inner_scope
							  = ctx.query<compiler::helios::QueryPrimaryCodeScopeFor>(inner_expr);
						  ASSERT_EQUAL(expected_scope, inner_scope);

						  for (auto sub_inner: inner_expr.unlock(ctx)->viewSubElements()) {
							  variant_match(sub_inner) {
								  variant_case(pst::LangElement::Child, sub_expr) {
									  sub_test_expr(sub_expr);
								  }
								  variant_default {}
							  }
						  }
					  };

				sub_test_expr(expr);
			};

			auto all_expr_holders
				= pst::viewAllSubTreeElementsFilter<pst::ExprHolder>(pst->getRootElement());

			// We test that each expr_holder and all its sub expressions
			// have the same scope as their "top expr_holder"
			for (auto expr_locked: all_expr_holders) {
				auto expr_holder = expr_locked.unlock(ctx);
				if (expr_holder->isTopLevel()) {
					// test all sub elements:
					test_expr(expr_holder);
				} else {
					// test that element has a top-expr parent
					auto element = expr_holder->getParent().value().unlock(ctx);

					while (true) {
						if (auto holder = element.dynamicCast<pst::ExprHolder>()) {
							if (holder.value()->isTopLevel()) {
								// OK, we found parent
								break;
							}
						}
						// if parent doesn't exist, we will
						// hit panic here at some point:
						element = element->getParent().value().unlock(ctx);
					}
				}
			}
		});
	}

	void testFunctionCallExpr() {
		auto [module, scope] = getModule(fs::File(path("test_modules/function_calls")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		ASSERT_EQUAL(4, hout.functions.size());
		auto function = hout.functions.at(1);
		ASSERT_EQUAL(function->declaration->original_name, "foo");
		auto variable = dynamic_cast<const compiler::helios::code::VariableStmt*>(
			function->body->statements.at(0).ref().get()
		);
		ASSERT_TRUE(variable != nullptr);
		auto call_expr
			= dynamic_cast<const compiler::helios::code::CallExpr*>(variable->initial_value.get());
		ASSERT_TRUE(call_expr != nullptr);
		auto square_symbol = getChain("square", scope).back();
		ASSERT_EQUAL(
			square_symbol, compiler::helios::getIdentifierExprSymID(call_expr->callee.ref()).value()
		);

		// just for cov and to see if it does not throw:
		query::utils::withContextDo([&](query::Context& ctx) {
			std::stringstream ss;
			hout.debugPrint(ctx, ss);
		});
	}

	void testFunctions() {
		auto [module, scope] = getModule(fs::File(path("test_modules/functions")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		// modify it as needed:
		ASSERT_EQUAL(1, hout.functions.size());

		for (auto& function: hout.functions) {
			if (function->declaration->original_name == base::StrID("stmtBody1")) {
				ASSERT_EQUAL(1, function->body->statements.size());
				auto stmt        = function->body->statements.at(0).ref();
				Ref  stmt_casted = dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*stmt);
				auto ret_expr    = stmt_casted->value.get();
				auto ctv
					= query::entryPoint<compiler::helios::QueryEvaluateHOUTExpression>({ ret_expr })
				          .valueOrThrow();
				ASSERT_EQUAL(1, ctv.get<compiler::numeric_value::NumericValue>()->get<i64>());
			}
		}
	}

	void testStrings() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/strings")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function   = hout.functions.at(0);
		auto& statements = function->body->statements;
		using namespace compiler::helios::code;

		{
			// let ab = 'a' +: b;
			const auto& prepended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(1));
			const auto  prepended_expr
				= dynamic_cast<const CallExpr*>(prepended_stmt.initial_value.get());
			const auto prepended_callee
				= dynamic_cast<IdentifierExpr*>(prepended_expr->callee.get());
			assertEqual(
				compiler::helios::name(prepended_callee->symbol),
				base::StrID("builtin_string_prepended"),
				"The prepended expression should call builtin_string_prepended"
			);
		}

		{
			// let bcd = b :+ 'c' :+ 'd';
			const auto& appended_stmt = dynamic_cast<const VariableStmt&>(*statements.at(2));
			const auto  appended_expr
				= dynamic_cast<const CallExpr*>(appended_stmt.initial_value.get());
			const auto appended_callee = dynamic_cast<IdentifierExpr*>(appended_expr->callee.get());
			assertEqual(
				compiler::helios::name(appended_callee->symbol),
				base::StrID("builtin_string_appended"),
				"The appended expression should call builtin_string_appended"
			);
		}

		{
			// let helloWorld = hello ++ world;
			const auto& concatenated_stmt = dynamic_cast<const VariableStmt&>(*statements.at(5));
			const auto  concatenated_expr
				= dynamic_cast<const CallExpr*>(concatenated_stmt.initial_value.get());
			const auto concatenated_callee
				= dynamic_cast<IdentifierExpr*>(concatenated_expr->callee.get());
			assertEqual(
				compiler::helios::name(concatenated_callee->symbol),
				base::StrID("builtin_string_concatenated"),
				"The prepended expression should call builtin_string_concatenated"
			);
		}
	}

	void testStaticArrays() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/static_arrays")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions.at(0);

		auto& statements = function->body->statements;

		using namespace compiler::helios::code;

		{
			// matrix[0][1] = 42
			auto& assign_matrix = dynamic_cast<const AssignmentStmt&>(*statements.at(1));
			// matrix[0][1] -> IndexExpr(IndexExpr(matrix))
			auto outer_index = dynamic_cast<const IndexExpr*>(assign_matrix.location_expr.get());
			ASSERT_TRUE(outer_index != nullptr);
			auto inner_index = dynamic_cast<const IndexExpr*>(outer_index->base.get());
			ASSERT_TRUE(inner_index != nullptr);

			auto i32_type = getIntegralTypeNoContext(
				32, compiler::tsh::IntegralAbstractType::Signedness::Signed
			);
			ASSERT_EQUAL(outer_index->expression_type.getSymbolType().getType(), i32_type);
		}
		{
			// poly.vertices[1].x = 100
			auto& assign_poly = dynamic_cast<const AssignmentStmt&>(*statements.at(3));
			// poly.vertices[1].x -> Access(Index(Access(poly)))
			auto field_access_x = dynamic_cast<const AccessExpr*>(assign_poly.location_expr.get());
			ASSERT_TRUE(field_access_x != nullptr);
			ASSERT_EQUAL(compiler::helios::name(field_access_x->field), "x");

			auto index_access = dynamic_cast<const IndexExpr*>(field_access_x->base.get());
			ASSERT_TRUE(index_access != nullptr);

			auto field_access_vertices = dynamic_cast<const AccessExpr*>(index_access->base.get());
			ASSERT_TRUE(field_access_vertices != nullptr);
			ASSERT_EQUAL(compiler::helios::name(field_access_vertices->field), "vertices");
		}

		{
			// Constants in array sizes.
			auto& matrix_decl = dynamic_cast<const VariableStmt&>(*statements.at(0));
			auto  matrix_type = matrix_decl.type.getType();
			ASSERT_EQUAL(matrix_type.getKind(), compiler::tsh::Kind::StaticArray);
			auto static_arr = matrix_type.as<compiler::tsh::StaticArrayAbstractType>();
			ASSERT_EQUAL(static_arr.getSize(), 3);
			ASSERT_EQUAL(
				static_arr.getElementType().getType().getKind(), compiler::tsh::Kind::StaticArray
			);
			auto static_arr_inner
				= static_arr.getElementType().getType().as<compiler::tsh::StaticArrayAbstractType>();
			ASSERT_EQUAL(static_arr_inner.getSize(), 2);
		}
	}

	void testDynamicArrays() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/dynamic_arrays")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function   = hout.functions.at(0);
		auto& statements = function->body->statements;

		using namespace compiler::helios::code;
		using namespace compiler::tsh;

		{
			// var l: List[i64];
			auto& var_decl = dynamic_cast<const VariableStmt&>(*statements.at(0));
			ASSERT_EQUAL(compiler::helios::name(var_decl.helios_symbol), "l");

			auto type = var_decl.type.getType();
			ASSERT_EQUAL(type.getKind(), Kind::DynamicArray);

			auto dyn_array_type = type.as<DynamicArrayAbstractType>();
			auto i64_type       = getIntegralTypeNoContext(
                64, compiler::tsh::IntegralAbstractType::Signedness::Signed
            );
			ASSERT_EQUAL(dyn_array_type.getElementType().getType(), i64_type);

			auto* default_val = dynamic_cast<const DefaultValueExpr*>(var_decl.initial_value.get());
			ASSERT_TRUE(default_val != nullptr);
		}
		{
			// l += 1;
			auto& expr_stmt = dynamic_cast<const ExprStmt&>(*statements.at(1));
			auto* push_expr = dynamic_cast<const ListPushExpr*>(expr_stmt.expr.get());
			ASSERT_TRUE(push_expr != nullptr);
		}
		{
			// l -= 1;
			auto& expr_stmt = dynamic_cast<const ExprStmt&>(*statements.at(2));
			auto* pop_expr  = dynamic_cast<const ListPopExpr*>(expr_stmt.expr.get());
			ASSERT_TRUE(pop_expr != nullptr);
		}
		{
			// let l_len = len l;
			auto& var_decl = dynamic_cast<const VariableStmt&>(*statements.at(3));
			auto* len_expr = dynamic_cast<const UnaryOperatorExpr*>(var_decl.initial_value.get());
			ASSERT_TRUE(len_expr != nullptr);
			ASSERT_EQUAL(len_expr->operation, BuiltinUnary::Len);
		}
		{
			// l[0] = 42;
			auto& assign_stmt = dynamic_cast<const AssignmentStmt&>(*statements.at(4));
			auto* index_expr  = dynamic_cast<const IndexExpr*>(assign_stmt.location_expr.get());
			ASSERT_TRUE(index_expr != nullptr);
		}
		{
			// let x = l[0];
			auto& var_decl   = dynamic_cast<const VariableStmt&>(*statements.at(5));
			auto* index_expr = dynamic_cast<const IndexExpr*>(var_decl.initial_value.get());
			ASSERT_TRUE(index_expr != nullptr);
		}
	}

	void testBuiltinFunctions() {
		auto [module, scope] = getModule(fs::File(path("test_modules/builtins")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		ASSERT_EQUAL(1, hout.functions.size());

		auto function = hout.functions.at(0);
		ASSERT_EQUAL(function->declaration->original_name, "main");

		Ref variable_stmt = dynamic_cast<const compiler::helios::code::VariableStmt*>(
			function->body->statements.at(0).ref().get()
		);
		Ref call_expr_1 = dynamic_cast<const compiler::helios::code::CallExpr*>(
			variable_stmt->initial_value.get()
		);
		auto call_expr_1_callee
			= compiler::helios::getIdentifierExprSymID(call_expr_1->callee.ref()).value();
		ASSERT_TRUE(std::holds_alternative<compiler::helios::builtin::BuiltinFunctionData>(
			getSymRef(call_expr_1_callee)->other
		));
		ASSERT_EQUAL(base::StrID("builtin_input_i64"), compiler::helios::name(call_expr_1_callee));

		Ref expr_stmt = dynamic_cast<const compiler::helios::code::ExprStmt*>(
			function->body->statements.at(1).ref().get()
		);
		Ref  call_expr_2 = dynamic_cast<const compiler::helios::code::CallExpr*>(&*expr_stmt->expr);
		auto call_expr_2_callee
			= compiler::helios::getIdentifierExprSymID(call_expr_2->callee.ref()).value();
		ASSERT_TRUE(std::holds_alternative<compiler::helios::builtin::BuiltinFunctionData>(
			getSymRef(call_expr_2_callee)->other
		));
		ASSERT_EQUAL(base::StrID("builtin_output_i64"), compiler::helios::name(call_expr_2_callee));
		auto& builtin_output_decl
			= query::entryPoint<compiler::helios::QueryDeclOfFun>(call_expr_2_callee)->valueOrPanic();
		ASSERT_EQUAL(
			builtin_output_decl.parameters.at(0).type.getType().getKind(),
			compiler::tsh::Kind::Integral
		);
	}

	void testFunctionReturnTypeDeduction() {
		auto [module, scope] = getModule(fs::File(path("test_modules/return_deduction")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto i64_type = getIntegralTypeNoContext(64, Signed);

		for (auto& function: hout.functions)
			ASSERT_EQUAL(function->declaration->return_type.getType(), i64_type);
	}

	void testFunctionReturnTypeCheckAndCoercion() {
		auto [module, scope] = getModule(fs::File(path("test_modules/return_coercion")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto i64_type = getIntegralTypeNoContext(64, Signed);

		for (auto& function: hout.functions) {
			ASSERT_EQUAL(i64_type, function->declaration->return_type.getType());

			if (function->declaration->original_name == base::StrID("big_example")) {
				for (size_t i: std::initializer_list<size_t>{ 1, 2, 3, 4 }) {
					auto if_stmt = function->body->statements.at(i).ref();
					auto if_stmt_casted
						= dynamic_cast<const compiler::helios::code::IfStmt*>(&*if_stmt);
					if (!if_stmt_casted) {
						// we only test if-statements here
						continue;
					}

					auto ret_stmt = if_stmt_casted->then_body.statements.at(0).ref();
					auto ret_stmt_casted
						= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
					assertTrue(ret_stmt_casted != nullptr, "Return statement expected.");

					auto ret_type = ret_stmt_casted->value->expression_type.getType();
					ASSERT_EQUAL(function->declaration->return_type.getType(), ret_type);

					auto cast_expr = dynamic_cast<const compiler::helios::code::CastExpr*>(
						ret_stmt_casted->value.get()
					);
					assertTrue(cast_expr != nullptr, "Cast expression expected.");
				}
				continue;
			}

			auto ret_stmt = function->body->statements.back().ref();
			auto ret_stmt_casted
				= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
			assertTrue(ret_stmt_casted != nullptr, "Return statement expected.");

			auto ret_type = ret_stmt_casted->value->expression_type.getType();
			ASSERT_EQUAL(function->declaration->return_type.getType(), ret_type);

			auto cast_expr
				= dynamic_cast<const compiler::helios::code::CastExpr*>(ret_stmt_casted->value.get()
			    );
			assertTrue(cast_expr != nullptr, "Cast expression expected.");
		}
	}

	void testMethodCalls() {
		auto [module, scope] = getModule(fs::File(path("test_modules/method_calls")));

		auto example_class = getChain("ExampleClass", scope).back();
		auto example_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(example_class)
		          ->valueOrThrow();
		auto example_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(example_class)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();
		ASSERT_EQUAL(3, example_class_info.methods.size());

		for (auto& method: example_class_info.methods) {
			auto method_hout
				= query::entryPoint<compiler::helios::QueryCodeOfFun>({ method })->valueOrPanic();
			ASSERT_EQUAL(
				refst(example_class_abstract_type), method_hout.declaration->parameters.at(0).type
			);
		}

		auto wrapper_class = getChain("Wrapper", scope).back();
		auto wrapper_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(wrapper_class)
		          ->valueOrThrow();
		auto wrapper_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(wrapper_class)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();
		ASSERT_EQUAL(4, wrapper_class_info.methods.size());

		for (auto& method: wrapper_class_info.methods) {
			auto method_hout
				= query::entryPoint<compiler::helios::QueryCodeOfFun>({ method })->valueOrPanic();
			ASSERT_EQUAL(
				refst(wrapper_class_abstract_type), method_hout.declaration->parameters.at(0).type
			);
		}

		auto point_class = getChain("Point", scope).back();
		auto point_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(point_class)->valueOrThrow();
		auto point_class_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(point_class)
		          ->valueOrThrow()
		          .getType()
		          .as<compiler::tsh::ClassAbstractType>();
		ASSERT_EQUAL(7, point_class_info.methods.size());

		for (auto& method: point_class_info.methods) {
			auto method_hout
				= query::entryPoint<compiler::helios::QueryCodeOfFun>({ method })->valueOrPanic();
			ASSERT_EQUAL(
				refst(point_class_abstract_type), method_hout.declaration->parameters.at(0).type
			);
		}

		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module })
		          .valueOrPanic();
		(void) units;  // @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		               // that it is successful
	}

	void testMangler() {
		auto [module, _] = getModule(fs::File(path("test_modules/mangling")));
		const auto& hout_unit
			= query::entryPoint<compiler::helios::QueryModuleHOUT>(module)->valueOrPanic();

		auto find_function = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                     ) -> base::Optional<CRef<compiler::helios::HOUTFunction>> {
			for (const auto& fun: unit.functions)
				if (fun->declaration->original_name == name) return fun;
			fail(base::strConcat("Function ", name.strView(), " not found"));
			return {};
		};

		auto find_global = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                   ) -> base::Optional<CRef<compiler::helios::HOUTGlobalData>> {
			for (const auto& glob: unit.glob_data)
				if (glob->original_name == name) return glob;
			assertTrue(false, base::strConcat("Global ", name.strView(), " not found"));
			return {};
		};

		auto goo = find_function(hout_unit, base::StrID("goooo")).value();
		std::cerr << "\nFunction name: " << goo->declaration->original_name.strView() << '\n';
		auto mangled_goo = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = goo->declaration->original_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 123,
		      .additional_metadata     = "metadata_v123" }
		);
		std::cerr << "Mangled symbol: " << mangled_goo.strView() << '\n';

		auto glob_a = find_global(hout_unit, base::StrID("A")).value();
		std::cerr << "\nGlobal variable name: " << glob_a->original_name.strView() << '\n';
		auto mangled_glob_a = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = glob_a->helios_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 0,
		      .additional_metadata     = std::nullopt }
		);
		std::cerr << "Mangled symbol: " << mangled_glob_a.strView() << '\n';
		ASSERT_EQUAL("_Q_M8manglingG1A", mangled_glob_a.str());

		auto glob_b = find_global(hout_unit, base::StrID("B")).value();
		std::cerr << "\nGlobal Variable name: " << glob_b->original_name.strView() << '\n';
		auto mangled_glob_b = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = glob_b->helios_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 321,
		      .additional_metadata     = "metadata_v321" }
		);
		std::cerr << "Mangled symbol: " << mangled_glob_b.strView() << '\n';
		ASSERT_EQUAL("_Q5a_M8manglingN5Nmspc1BE$metadata_v321", mangled_glob_b.str());

		auto g_const = find_global(hout_unit, base::StrID("Cnst")).value();
		std::cerr << "\nConst name: " << g_const->original_name.strView() << '\n';
		auto mangled_g_const = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = g_const->helios_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 321,
		      .additional_metadata     = "metadata_v321" }
		);
		std::cerr << "Mangled symbol: " << mangled_g_const.strView() << '\n';


		auto sub_module_a = query::utils::withContextCompute([&](query::Context& ctx) {
			return compiler::frontend::getModuleRef(module)
			    ->getSubmoduleByName(base::StrID{ "sub" })
			    .unlock(ctx)
			    ->unlock(ctx)
			    .getID();
		});
		auto sub_module   = std::any_cast<compiler::frontend::ModuleID>(sub_module_a);

		const auto& sub_hout_unit
			= query::entryPoint<compiler::helios::QueryModuleHOUT>(sub_module)->valueOrPanic();

		auto sub_fun = find_function(sub_hout_unit, base::StrID("subFun")).value();
		std::cerr << "\nSub function name: " << sub_fun->declaration->original_name.strView()
				  << '\n';
		auto mangled_sub_fun = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = sub_fun->declaration->original_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 5,
		      .additional_metadata     = "metadata_v5" }
		);
		std::cerr << "Mangled symbol: " << mangled_sub_fun.strView() << '\n';

		auto sub_cnst = find_global(sub_hout_unit, base::StrID("subConst")).value();
		std::cerr << "\nSub constant name: " << sub_cnst->original_name.strView() << '\n';
		auto mangled_sub_cnst = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key              = sub_cnst->helios_symbol,
		      .kind                    = compiler::helios::mangler::ManglingSymbolKind::Standard,
		      .mangling_scheme_version = 5,
		      .additional_metadata     = "metadata_v5" }
		);
		std::cerr << "Mangled symbol: " << mangled_sub_cnst.strView() << '\n';

		ASSERT_EQUAL(
			"_Q1Y_M8manglingN4Mspc3Ooo5gooooEFi32i32f64E1a1bE$metadata_v123", mangled_goo.str()
		);
		ASSERT_EQUAL("_Q5a_M8manglingN5Nmspc1BE$metadata_v321", mangled_glob_b.str());

		ASSERT_EQUAL("_Q_M8manglingG1A", mangled_glob_a.str());

		ASSERT_EQUAL("_Q5a_M8manglingN4Mspc3Ooo4CnstE$metadata_v321", mangled_g_const.str());

		ASSERT_EQUAL("_Q4_M8mangling3subN5inSub6subFunEFi32EE$metadata_v5", mangled_sub_fun.str());
		ASSERT_EQUAL("_Q4_M8mangling3subN5inSub8subConstE$metadata_v5", mangled_sub_cnst.str());
	}

	void testManglerSpecialMembers() {
		std::cerr << "--- testManglerSpecialMembers ---\n";

		auto [module, root_scope] = getModule(fs::File(path("test_modules/mangling_special_mem")));

		auto variable_a       = getChain("A", root_scope);
		auto variable_b       = getChain("M.B", root_scope);
		auto mangled_a_constr = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key = variable_a.back(),
		      .kind = compiler::helios::mangler::ManglingSymbolKind::GlobalVariableConstructor,
		      .mangling_scheme_version = 5,
		      .additional_metadata     = std::nullopt }
		);
		auto mangled_a_destr = query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
			{ .symbol_key = variable_a.back(),
		      .kind       = compiler::helios::mangler::ManglingSymbolKind::GlobalVariableDestructor,
		      .mangling_scheme_version = 5,
		      .additional_metadata     = std::nullopt }
		);
		ASSERT_EQUAL("_Q4_M20mangling_special_memG1Agc", mangled_a_constr.str());
		ASSERT_EQUAL("_Q4_M20mangling_special_memG1Agd", mangled_a_destr.str());

		// Using the "getSpecialMangledName" aliases:
		query::utils::withContextDo([&](query::Context& ctx) {
			auto mangled_b_constr = compiler::helios::mangler::getSpecialMangledName<
				compiler::helios::mangler::ManglingSymbolKind::GlobalVariableConstructor>(
				ctx, variable_b.back()
			);
			auto mangled_b_destr = compiler::helios::mangler::getSpecialMangledName<
				compiler::helios::mangler::ManglingSymbolKind::GlobalVariableDestructor>(
				ctx, variable_b.back()
			);

			ASSERT_EQUAL("_Q_M20mangling_special_memN1M1BEgc", mangled_b_constr.str());
			ASSERT_EQUAL("_Q_M20mangling_special_memN1M1BEgd", mangled_b_destr.str());
		});
	}

	void testGlobalVariableExpressions() {
		auto find_function = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                     ) -> base::Optional<CRef<compiler::helios::HOUTFunction>> {
			for (const auto& fun: unit.functions)
				if (fun->declaration->original_name == name) return fun;
			fail(base::strConcat("Function ", name.strView(), " not found"));
			return {};
		};

		auto find_global = [&](const compiler::helios::HOUTUnit& unit, const base::StrID& name
		                   ) -> base::Optional<CRef<compiler::helios::HOUTGlobalData>> {
			for (const auto& glob: unit.glob_data)
				if (glob->original_name == name) return glob;
			assertTrue(false, base::strConcat("Global ", name.strView(), " not found"));
			return {};
		};

		{  // General global variable checks.

			auto [module, _] = getModule(fs::File(path("test_modules/global_variables/general")));
			const auto& hout_unit
				= query::entryPoint<compiler::helios::QueryModuleHOUT>(module)->valueOrPanic();

			ASSERT_EQUAL(hout_unit.glob_data.size(), 3);

			auto glob1 = find_global(hout_unit, base::StrID("B")).value();
			auto glob2 = find_global(hout_unit, base::StrID("XB")).value();

			CRef<compiler::helios::code::Expr> expr1
				= std::get<compiler::helios::HOUTGlobalVariable>(glob1->value).initial_value.ref();
			CRef<compiler::helios::code::Expr> expr2
				= std::get<compiler::helios::HOUTGlobalVariable>(glob2->value).initial_value.ref();

			ASSERT_EQUAL(
				compiler::helios::code::BuiltinBinary::IntegerAdd,
				dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(&*expr1)->operation
			);
			ASSERT_EQUAL(
				compiler::helios::getIdentifierExprSymID(
					dynamic_cast<const compiler::helios::code::CallExpr*>(&*expr2)->callee.ref()
				)
					.value(),
				find_function(hout_unit, base::StrID("foooo")).value()->declaration->original_symbol
			);
			expr1->debugPrint(std::cerr);
			std::cerr << '\n';
		}

		{  // Global variable detections check (isGlobalVar function).
			auto [module, _] = getModule(fs::File(path("test_modules/global_variables/detection")));
			const auto& hout_unit
				= query::entryPoint<compiler::helios::QueryModuleHOUT>(module)->valueOrPanic();

			ASSERT_EQUAL(hout_unit.glob_data.size(), 4);

			std::vector<std::string> globals = { "A", "B", "C", "A_in_expand" };
			query::utils::withContextDo([&](query::Context& ctx) {
				for (const auto& name: globals)
					ASSERT_TRUE(compiler::helios::isGlobalVar(
						ctx, find_global(hout_unit, base::StrID(name)).value()->helios_symbol
					));

				for (const auto& fun: hout_unit.functions) {
					if (fun->declaration->original_name.str() == "foo0") {
						auto var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
							fun->body->statements[0].get()
						);
						ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));
					}
					if (fun->declaration->original_name.str() == "foo1") {
						auto var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
							fun->body->statements[0].get()
						);
						ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));

						var_ptr = dynamic_cast<compiler::helios::code::VariableStmt*>(
							fun->body->statements[1].get()
						);
						ASSERT_TRUE(not compiler::helios::isGlobalVar(ctx, var_ptr->helios_symbol));
					}
				}
			});
		}
	}

	/**
	 * This checks if all consts and vars in the module have proper types.
	 */
	void testTypeOfConstAndVar() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/type_deduction")));

		const auto int32_type = getIntegralTypeNoContext(32, Signed);
		const auto int64_type = getIntegralTypeNoContext(64, Signed);

		const auto f32_type = getFloatTypeNoContext(32);
		const auto f64_type = getFloatTypeNoContext(64);

		const auto bool_type = compiler::tsh::getBoolType();

		const auto str_type = compiler::tsh::getStringType();

		const auto tuple_ii_type = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { st(int32_type), st(int32_type) } }
		);
		const auto tuple_si_type
			= query::entryPoint<compiler::tsh::QueryTupleType>({ { st(str_type), st(int32_type) } });

		auto foo            = getChain("foo", root_scope).back();
		auto foo_body_scope = getFunctionBodyScope(foo);

		// Vars
		ASSERT_EQUAL(int32_type, getTypeOf("EasyIntI32", foo_body_scope));
		ASSERT_EQUAL(int64_type, getTypeOf("EasyIntI64", foo_body_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("EasyBinIntI32", foo_body_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("EasyOctIntI32", foo_body_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("EasyHexIntI32", foo_body_scope));

		ASSERT_EQUAL(f32_type, getTypeOf("EasyFloatF32", foo_body_scope));
		ASSERT_EQUAL(f32_type, getTypeOf("EasyFloatF32_2", foo_body_scope));
		ASSERT_EQUAL(f64_type, getTypeOf("EasyFloatF64", foo_body_scope));

		ASSERT_EQUAL(bool_type, getTypeOf("EasyBool", foo_body_scope));
		ASSERT_EQUAL(str_type, getTypeOf("EasyString", foo_body_scope));

		ASSERT_EQUAL(tuple_ii_type, getTypeOf("TupleVII", foo_body_scope));
		ASSERT_EQUAL(tuple_si_type, getTypeOf("TupleVSI", foo_body_scope));

		// Consts
		ASSERT_EQUAL(int32_type, getTypeOf("SimpleIntI32", root_scope));
		ASSERT_EQUAL(int64_type, getTypeOf("SimpleIntI64", root_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("SimpleBinIntI32", root_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("SimpleOctIntI32", root_scope));
		ASSERT_EQUAL(int32_type, getTypeOf("SimpleHexIntI32", root_scope));

		ASSERT_EQUAL(f32_type, getTypeOf("SimpleFloatF32", root_scope));
		ASSERT_EQUAL(f32_type, getTypeOf("SimpleFloatF32_2", root_scope));
		ASSERT_EQUAL(f64_type, getTypeOf("SimpleFloatF64", root_scope));

		ASSERT_EQUAL(bool_type, getTypeOf("SimpleBool", root_scope));
		ASSERT_EQUAL(str_type, getTypeOf("SimpleString", root_scope));

		ASSERT_EQUAL(tuple_ii_type, getTypeOf("TupleCII", root_scope));
		ASSERT_EQUAL(tuple_si_type, getTypeOf("TupleCSI", root_scope));

		// Differences between const, let, and var
		const auto const_type = getSymbolTypeOf("const_no_type", root_scope);
		const auto let_type   = getSymbolTypeOf("let_no_type", root_scope);
		const auto var_type   = getSymbolTypeOf("var_no_type", root_scope);
		ASSERT_EQUAL(const_type.getType(), int32_type);
		ASSERT_EQUAL(let_type.getType(), int32_type);
		ASSERT_EQUAL(var_type.getType(), int32_type);
		ASSERT_EQUAL(const_type.getMutability(), compiler::tsh::Mutability::Immutable);
		ASSERT_EQUAL(let_type.getMutability(), compiler::tsh::Mutability::Immutable);
		ASSERT_EQUAL(var_type.getMutability(), compiler::tsh::Mutability::Mutable);
	}

	void testDebugPrint() {
		auto [module, scope] = getModule(fs::File(path("test_modules/pretty_debug")));

		auto sym_c1         = getChain("c1", scope).back();
		auto sym_v1         = getChain("v1", scope).back();
		auto sym_n1         = getChain("n1", scope).back();
		auto sym_n2         = getChain("n1.n2", scope).back();
		auto sym_test_class = getChain("n1.n2.TestClass", scope).back();
		auto sym_n3         = getChain("n1.n3", scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			std::cout << compiler::helios::prettyDebugPrint(sym_c1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_v1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n1, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n2, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_test_class, ctx) << '\n';
			std::cout << compiler::helios::prettyDebugPrint(sym_n3, ctx) << '\n';
		});
	}

	void testStmtSpecifiers() {
		// Utility functions for testing statement specifiers
		auto has_specifier
			= [](query::Context&                                           ctx,
		         const std::vector<pst::AccessLocked<pst::StmtSpecifier>>& specifiers,
		         pst::Keyword                                              keyword) -> bool {
			for (const auto& spec: specifiers) {
				auto unlocked = spec.unlock(ctx);
				if (unlocked->getSpecifier() == keyword) return true;
			}
			return false;
		};

		auto verify_specifier_order
			= [](query::Context&                                           ctx,
		         const std::vector<pst::AccessLocked<pst::StmtSpecifier>>& specifiers,
		         const std::vector<pst::Keyword>&                          expected_order) -> bool {
			if (specifiers.size() != expected_order.size()) return false;

			for (size_t i = 0; i < specifiers.size(); ++i) {
				auto unlocked = specifiers[i].unlock(ctx);
				if (unlocked->getSpecifier() != expected_order[i]) return false;
			}
			return true;
		};

		auto test_c_abi_with_library = [this](
										   query::Context&                  ctx,
										   compiler::helios::SymID          symbol,
										   base::Optional<std::string_view> expected_library
									   ) {
			auto abi_value = ctx.query<compiler::helios::QuerySymbolABI>(symbol)->valueOrThrow();
			ASSERT_TRUE(std::holds_alternative<compiler::helios::CAbi>(abi_value));
			auto c_abi = std::get<compiler::helios::CAbi>(abi_value);
			if (!expected_library.empty()) {
				ASSERT_TRUE(c_abi.library.has_value());
				ASSERT_EQUAL(expected_library, c_abi.library.value().strView());
			}
		};

		auto test_default_abi = [this](query::Context& ctx, compiler::helios::SymID symbol) {
			auto abi_value = ctx.query<compiler::helios::QuerySymbolABI>(symbol)->valueOrThrow();
			ASSERT_TRUE(std::holds_alternative<compiler::helios::DefaultAbi>(abi_value));
		};
		auto [module, root_scope] = getModule(fs::File(path("test_modules/stmt_specifiers")));

		// Test functions with different specifiers
		auto c_function           = getChain("cFunction", root_scope).back();
		auto private_function     = getChain("privateFunction", root_scope).back();
		auto public_function      = getChain("publicFunction", root_scope).back();
		auto c_private_function   = getChain("cPrivateFunction", root_scope).back();
		auto lib_function         = getChain("libFunction", root_scope).back();
		auto invalid_abi_function = getChain("invalidAbiFunction", root_scope).back();

		// Test symbols from extern("C") block
		auto c_block_function        = getChain("cBlockFunction", root_scope).back();
		auto c_block_public_function = getChain("cBlockPublicFunction", root_scope).back();

		// Test struct
		auto regular_struct = getChain("RegularStruct", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			// Test QuerySpecifiersOfSymbol with order verification
			{
				// Test extern("C") function - should have extern specifier
				auto specifiers = ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(c_function);
				ASSERT_EQUAL(1, specifiers->size());
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Extern));
			}

			{
				// Test private function
				auto specifiers
					= ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(private_function);
				ASSERT_EQUAL(1, specifiers->size());
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Private));
			}

			{
				// Test public function
				auto specifiers
					= ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(public_function);
				ASSERT_EQUAL(1, specifiers->size());
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Public));
			}

			{
				// Test extern("C") private function - verify order
				auto specifiers
					= ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(c_private_function);
				ASSERT_EQUAL(2, specifiers->size());
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Extern));
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Private));
				ASSERT_TRUE(verify_specifier_order(
					ctx, *specifiers, { pst::Keyword::Private, pst::Keyword::Extern }
				));
			}

			// Test QuerySymbolABI for various cases
			{
				// Test extern("C") function - should have C ABI with no library
				test_c_abi_with_library(ctx, c_function, {});
			}

			{
				// Test extern("C", "mylib") function - should have C ABI with mylib
				test_c_abi_with_library(ctx, lib_function, "mylib");
			}

			{
				// Test private function - should have default ABI
				test_default_abi(ctx, private_function);
			}

			{
				// Test public function - should have default ABI
				test_default_abi(ctx, public_function);
			}

			{
				// Test extern("C") private function - should have C ABI
				test_c_abi_with_library(ctx, c_private_function, {});
			}

			{
				// Test regular struct - should have default ABI
				test_default_abi(ctx, regular_struct);
			}

			// Test extern("C") block symbols
			{
				// Test function inside extern("C") block - should have C ABI and extern specifier
				auto specifiers
					= ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(c_block_function);
				ASSERT_TRUE(specifiers->size() >= 1);
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Extern));
				test_c_abi_with_library(ctx, c_block_function, {});
			}

			{
				// Test public function inside extern("C") block - should have C ABI, extern and
				// public specifiers
				auto specifiers
					= ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(c_block_public_function);
				ASSERT_TRUE(specifiers->size() >= 2);
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Extern));
				ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Public));
				test_c_abi_with_library(ctx, c_block_public_function, {});
			}

			// Test invalid ABI function - should fail
			try {
				ctx.query<compiler::helios::QuerySymbolABI>(invalid_abi_function)->valueOrThrow();
				CORE_PANIC("Should throw for invalid ABI.");
			} catch (query::internal::QueryFailedException& err) {
				// Expected failure for invalid ABI
			}
		});
	}

	void testOverloadResolution() {
		auto [module, root_scope] = getModule(fs::File(path("test_modules/overload_resolution")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto get_function_by_order
			= [&](usize index) { return hout.functions.at(index)->declaration->original_symbol; };

		// Store function symbols for each overload (order matches declaration order in file)
		auto foo_bool  = get_function_by_order(0);  // fun foo(x: bool)
		auto foo_float = get_function_by_order(1);  // fun foo(x: f64)
		auto foo_class = get_function_by_order(2);  // fun foo(x: MyClass)
		auto foo_i64   = get_function_by_order(3);  // fun foo(x: i64)
		auto goo_x     = get_function_by_order(4);  // fun goo(x: i64) -> i32 (first one)
		auto goo_y     = get_function_by_order(5);  // fun goo(y: i64) -> i32 (second one)
		auto goo_f64   = get_function_by_order(6);  // fun goo(x: f64, y: bool) -> i64
		[[maybe_unused]] auto goo_i64
			= get_function_by_order(7);             // fun goo(x: i64, y: bool) -> i64

		// Helper to get the function symbol called in a global variable's initializer
		auto get_function_sym_by_var_sym = [](auto var_sym) {
			auto expr      = getExprOfVariable(var_sym);
			Ref  call_expr = dynamic_cast<const compiler::helios::code::CallExpr*>(&*expr);

			Ref ident_expr = dynamic_cast<const compiler::helios::code::IdentifierExpr*>(
				&*call_expr->callee.ref()
			);
			return ident_expr->symbol;
		};

		// Test overload resolution by argument type
		auto call_foo_bool_sym  = getChain("CALL_FOO_BOOL", root_scope).back();
		auto call_foo_float_sym = getChain("CALL_FOO_FLOAT", root_scope).back();
		auto call_foo_class_sym = getChain("CALL_FOO_CLASS", root_scope).back();
		auto call_foo_i64_sym   = getChain("CALL_FOO_I64", root_scope).back();

		ASSERT_EQUAL(foo_bool, get_function_sym_by_var_sym(call_foo_bool_sym));
		ASSERT_EQUAL(foo_float, get_function_sym_by_var_sym(call_foo_float_sym));
		ASSERT_EQUAL(foo_class, get_function_sym_by_var_sym(call_foo_class_sym));
		ASSERT_EQUAL(foo_i64, get_function_sym_by_var_sym(call_foo_i64_sym));

		// Test overload resolution by named parameters
		auto call_goo_x_sym = getChain("CALL_GOO_X", root_scope).back();
		auto call_goo_y_sym = getChain("CALL_GOO_Y", root_scope).back();

		ASSERT_EQUAL(goo_x, get_function_sym_by_var_sym(call_goo_x_sym));
		ASSERT_EQUAL(goo_y, get_function_sym_by_var_sym(call_goo_y_sym));

		// Test overload resolution with coercion (f32 -> f64 is preferred over f32 -> i64)
		auto call_goo_f64_sym = getChain("CALL_GOO_F64", root_scope).back();
		ASSERT_EQUAL(goo_f64, get_function_sym_by_var_sym(call_goo_f64_sym));
	}

	void testDefaultInitializers() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/default_constructors")));

		auto trivial_sym      = getChain("Trivial", root_scope).back();
		auto with_init_sym    = getChain("WithInit", root_scope).back();
		auto nested_sym       = getChain("Nested", root_scope).back();
		auto holder_sym       = getChain("ArrayHolder", root_scope).back();
		auto deep_sym         = getChain("DeepStack", root_scope).back();
		auto deep_trivial_sym = getChain("DeepStackTrivial", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type = [&](SymID sym_id) {
				return ctx.query<QueryTypeFromDefinition>(sym_id)->valueOrThrow();
			};

			auto i32_st = st(compiler::tsh::getIntegralType(
				ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
			));

			// Primitives should be zero initialized.
			{
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(i32_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}

			// Unit type should be initialized with a unit literal.
			{
				auto        unit_st = st(compiler::tsh::getUnitType());
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(unit_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const LiteralUnitExpr*>(expr.get()) != nullptr);
			}

			// Meta type should be initialized with a type literal (void by default).
			{
				auto        meta_st = st(compiler::tsh::getMetaType());
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(meta_st)->valueOrThrow();
				auto        type_lit = dynamic_cast<const LiteralTypeExpr*>(expr.get());
				ASSERT_TRUE(type_lit != nullptr);
				ASSERT_EQUAL(compiler::tsh::getVoidType(), type_lit->value_type.getType());
			}

			// Trivial class should be zero initialized.
			{
				auto        trivial_st = get_class_type(trivial_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(trivial_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}

			// Class with an initial value provided for field should emit a call to a ctor.
			{
				auto        with_init_st = get_class_type(with_init_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(with_init_st)->valueOrThrow();

				auto call = dynamic_cast<const CallExpr*>(expr.get());
				ASSERT_TRUE(call != nullptr);

				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps     = ctx.query<QueryTransitiveFunctionCalls>(ctor_sym)->valueOrThrow();

				// Should not call any recursive ctors.
				ASSERT_EQUAL_PRINT(1, deps.size());
			}

			// Class with a class field which is non zero-initializable should emit a ctor call.
			// This ctor should call a ctor of the inner non zero-initializable field.
			{
				auto        nested_st = get_class_type(nested_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(nested_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps     = ctx.query<QueryTransitiveFunctionCalls>(ctor_sym)->valueOrThrow();

				// The top-level constructor should call one function which is a default ctor of
				// `WithInit`.
				ASSERT_EQUAL_PRINT(2, deps.size());

				auto dep_gsd = std::get<GeneratedSymbolData>(getSymRef(deps[0])->other);
				ASSERT_TRUE(std::holds_alternative<GeneratedSymbolData::DefaultClassConstructor>(
					dep_gsd.data
				));
			}

			// ArrayHolder ctor should call a ctor of static array field, which calls a ctor of the
			// inner element.
			{
				auto        holder_st = get_class_type(holder_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(holder_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps     = ctx.query<QueryTransitiveFunctionCalls>(ctor_sym)->valueOrThrow();

				// Ctor(ArrayHolder) -> Ctor(WithInit[5]) -> Ctor(WithInit)
				ASSERT_EQUAL_PRINT(3, deps.size());

				bool found_array_ctor = false;
				for (auto d: deps) {
					auto gsd = std::get<GeneratedSymbolData>(getSymRef(d)->other);
					if (std::holds_alternative<GeneratedSymbolData::DefaultStaticArrayConstructor>(
							gsd.data
						))
						found_array_ctor = true;
				}
				ASSERT_TRUE(found_array_ctor);
			}

			// `DeepStack` ctor should call a ctor of the `Nested` field, which calls a ctor of
			// `WithInit`
			{
				auto        deep_st = get_class_type(deep_sym);
				const auto& expr = ctx.query<QueryDefaultInitializerExpr>(deep_st)->valueOrThrow();

				auto call     = dynamic_cast<const CallExpr*>(expr.get());
				auto ctor_sym = getIdentifierExprSymID(call->callee.ref()).value();
				auto deps     = ctx.query<QueryTransitiveFunctionCalls>(ctor_sym)->valueOrThrow();

				// Ctor(DeepStack) -> Ctor(Nested) -> Ctor(WithInit)
				ASSERT_EQUAL_PRINT(3, deps.size());
			}

			// `DeepStackTrivial` ctor should not call any default constructors, since it stores
			// a static array of trivially zero-initializable types which can be zero initialized,
			// thus its zero-initializable.
			{
				auto        deep_trivial_st = get_class_type(deep_trivial_sym);
				const auto& expr
					= ctx.query<QueryDefaultInitializerExpr>(deep_trivial_st)->valueOrThrow();
				ASSERT_TRUE(dynamic_cast<const DefaultValueExpr*>(expr.get()) != nullptr);
			}
		});
	}

	void testCastsHout() {
		// Load the small test module we added under test_modules/casts
		auto [module, root_scope] = getModule(fs::File(path("test_modules/casts")));

		// Get HOUT for the module and find the function HOUT unit
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		auto& function = hout.functions[0];

		// Helper: find a VariableStmt by name in the function body and return its initializer expr
		auto get_var_init_expr
			= [&](const base::StrID& varname) -> CRef<compiler::helios::code::Expr> {
			for (const auto& st_box: function->body->statements) {
				if (auto var_ptr
				    = dynamic_cast<const compiler::helios::code::VariableStmt*>(st_box.get())) {
					if (compiler::helios::name(var_ptr->helios_symbol) == varname)
						return var_ptr->initial_value.ref();
				}
			}
			CORE_PANIC("Variable not found in function body");
		};

		// Use helper to fetch initializer expressions and assert CastExpr insertion
		{
			auto expr_ptr = get_var_init_expr(base::StrID("explicit"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			auto f64_type = getFloatTypeNoContext(64);
			ASSERT_EQUAL(f64_type, cast_ptr->target_type.getType());
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("widen"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			auto i64_type = getIntegralTypeNoContext(64, Signed);
			ASSERT_EQUAL(i64_type, cast_ptr->target_type.getType());
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("bool_as_int"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			auto i32_type = getIntegralTypeNoContext(32, Signed);
			ASSERT_EQUAL(i32_type, cast_ptr->target_type.getType());
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("int_as_bool"));
			auto cast_ptr
				= dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
		}
	}

	void testTypeLifting() {
		// Load the small test module we added under test_modules/units_and_tuples
		auto [module, root_scope] = getModule(fs::File(path("test_modules/units_and_tuples")));

		const auto unit1     = getChain("Unit1", root_scope).back();
		const auto unit2     = getChain("Unit2", root_scope).back();
		const auto unit_type = getChain("UnitType", root_scope).back();

		const auto int1     = getChain("Int1", root_scope).back();
		const auto int2     = getChain("Int2", root_scope).back();
		const auto int_type = getChain("IntType", root_scope).back();

		const auto int_type1     = getChain("IntType1", root_scope).back();
		const auto int_type2     = getChain("IntType2", root_scope).back();
		const auto int_type_type = getChain("IntTypeType", root_scope).back();

		const auto tuple_ii1     = getChain("TupleII1", root_scope).back();
		const auto tuple_ii2     = getChain("TupleII2", root_scope).back();
		const auto tuple_ii_type = getChain("TupleIIType", root_scope).back();

		const auto tuple_tt1     = getChain("TupleTT1", root_scope).back();
		const auto tuple_tt2     = getChain("TupleTT2", root_scope).back();
		const auto tuple_tt_type = getChain("TupleTTType", root_scope).back();

		const auto tuple_lift_error = getChain("TupleLiftError", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto check_types = [&](const compiler::helios::SymID     sym_id,
			                             const compiler::tsh::SymbolType<> expected_type,
			                             const std::string&                message) -> void {
				const auto symbol_value
					= ctx.query<compiler::helios::QueryConstValueOf>(sym_id).valueOrThrow();
				const auto actual_type
					= symbol_value.getTypeOfStoredValue(ctx).withMutability(Immutable);
				assertEqual(actual_type, expected_type, message);
			};

			const auto meta_st = st(compiler::tsh::getMetaType()).withMutability(Immutable);
			const auto unit_st = st(compiler::tsh::getUnitType()).withMutability(Immutable);
			const auto int_st
				= st(compiler::tsh::getIntegralType(ctx, 32, Signed)).withMutability(Immutable);
			const auto tuple_ii_st
				= st(ctx.query<compiler::tsh::QueryTupleType>(
						 { { int_st.withMutability(Mutable), int_st.withMutability(Mutable) } }
					 )
			    ).withMutability(Immutable);
			const auto tuple_tt_st
				= st(ctx.query<compiler::tsh::QueryTupleType>(
						 { { meta_st.withMutability(Mutable), meta_st.withMutability(Mutable) } }
					 )
			    ).withMutability(Immutable);

			check_types(unit1, unit_st, "Unit1 should be of unit type.");
			check_types(unit2, unit_st, "Unit2 should be of unit type.");
			check_types(unit_type, meta_st, "UnitType should be of meta type.");

			check_types(int1, int_st, "Int1 should be of integer type.");
			check_types(int2, int_st, "Int2 should be of integer type.");
			check_types(int_type, meta_st, "IntType should be of meta type.");

			check_types(int_type1, meta_st, "IntType1 should be of integer type.");
			check_types(int_type2, meta_st, "IntType2 should be of integer type.");
			check_types(int_type_type, meta_st, "IntTypeType should be of integer type.");

			check_types(tuple_ii1, tuple_ii_st, "TupleII1 should be of tuple type.");
			check_types(tuple_ii2, tuple_ii_st, "TupleII2 should be of tuple type.");
			check_types(tuple_ii_type, meta_st, "TupleIIType should be of meta type.");

			check_types(tuple_tt1, tuple_tt_st, "TupleTT1 should be of tuple type.");
			check_types(tuple_tt2, tuple_tt_st, "TupleTT2 should be of tuple type.");
			check_types(tuple_tt_type, meta_st, "TupleTTType should be of meta type.");

			assertTrue(
				ctx.query<compiler::helios::QueryConstValueOf>(tuple_lift_error).hasFailed(),
				"Trying to lift an unliftable tuple to a type should fail."
			);
			std::stringstream ss;
			ctx.dumpToOneLoggerAndClear()->terminalPrint(ss);
			assertTrue(
				ss.str().contains("cannot be converted"),
				"Trying to lift an unliftable tuple to a type should result in a coercion error."
			);
		});
	}

	/**
	 * Test the origin calculation for elements.
	 * The origin calculation is not trivial for expressions and generated elements.
	 */
	void testHoutElementsOrigin() {
		auto [module, _] = getModule(fs::File(path("test_modules/helios_pst_origin_tests")));

		auto& hout = query::entryPoint<compiler::helios::QueryModuleHOUT>(module)->valueOrPanic();

		auto get_global_by_name
			= [&](base::StrID name) -> base::Optional<CRef<compiler::helios::HOUTGlobalData>> {
			for (const auto& glob: hout.glob_data)
				if (glob->original_name == name) return glob;
			return {};
		};

		auto main_file = query::entryPoint<compiler::frontend::QueryMainSourceFile>({ module });
		base::Optional<CRef<pst::PST<>>> pst;
		query::utils::withContextDo([&](::query::Context& ctx) { pst = getFilePST(ctx, main_file); }
		);
		auto all_variables
			= pst::viewAllSubTreeElementsFilter<pst::Variable>(pst.value()->getRootElement());

		auto get_pst_variable_by_name
			= [&](base::StrID name) -> base::Optional<pst::Access<pst::Variable>> {
			for (const auto& var: all_variables)
				if (var.illegalAccess().value()->getName() == name) return var.illegalAccess();
			return {};
		};

		/**
		 * Check if the source position of the origin of initial value expression
		 * calculated from HOUT is the same as the source position of the whole PST init expression.
		 * This is not trivial for AccessChain and for generated elements. (derefs, casts)
		 */
		auto check_var_init_expr_origin = [&](base::StrID name, bool is_generated) {
			auto glob_opt = get_global_by_name(name);
			auto var_opt  = get_pst_variable_by_name(name);
			assertTrue(glob_opt.has_value(), "Global not found in HOUT");
			assertTrue(var_opt.has_value(), "Variable not found in PST");
			auto glob              = glob_opt.value();
			auto var_pst           = var_opt.value();
			auto initial_value_pst = var_pst->getValue().value().illegalAccess().value();
			auto initial_value_expr
				= std::get<compiler::helios::HOUTGlobalVariable>(glob->value).initial_value.ref();

			dia::SourcePosition expr_pos_from_origin
				= initial_value_expr->origin.getSourcePosition().value();
			assertEqual(
				expr_pos_from_origin,
				initial_value_pst->getSourcePosition(),
				base::strConcat(
					"The initial value expression PST node does not match for variable ",
					name.strView()
				)
			);

			assertEqual(
				initial_value_expr->origin.isGenerated(),
				is_generated,
				base::strConcat("Generated origin does not match for variable ", name.strView())
			);
		};
		check_var_init_expr_origin(base::StrID("var1"), false);
		check_var_init_expr_origin(base::StrID("var2"), false);
		check_var_init_expr_origin(base::StrID("var3"), false);
		check_var_init_expr_origin(base::StrID("var4"), false);
		check_var_init_expr_origin(base::StrID("var5"), false);
		check_var_init_expr_origin(base::StrID("var6"), false);
		check_var_init_expr_origin(base::StrID("var7_generated"), true);
		check_var_init_expr_origin(base::StrID("var8_generated"), true);

		auto get_hout_function_by_name
			= [&](base::StrID name) -> base::Optional<CRef<compiler::helios::HOUTFunction>> {
			for (const auto& fun: hout.functions)
				if (fun->declaration->original_name == name) return fun;
			return {};
		};
		auto all_pst_functions
			= pst::viewAllSubTreeElementsFilter<pst::Fun>(pst.value()->getRootElement());
		auto get_pst_function_by_name
			= [&](base::StrID name) -> base::Optional<pst::Access<pst::Fun>> {
			for (const auto& fun: all_pst_functions)
				if (fun.illegalAccess().value()->getName() == name) return fun.illegalAccess();
			return {};
		};

		/**
		 * Check if the source position of the origin of function body calculated from HOUT
		 * is the same as the source position of the whole PST function of the same name.
		 */
		auto check_function_origin = [&](base::StrID name) {
			auto fun_opt     = get_hout_function_by_name(name);
			auto pst_fun_opt = get_pst_function_by_name(name);
			assertTrue(fun_opt.has_value(), "Function not found in HOUT");
			assertTrue(pst_fun_opt.has_value(), "Function not found in PST");
			auto fun = fun_opt.value();


			auto pst_fun = pst_fun_opt.value();
			assertEqual(
				fun->origin.getSourcePosition(),
				pst_fun->getSourcePosition(),
				base::strConcat("The function origin is not the PST of the function", name.strView())
			);
			assertEqual(
				fun->origin.getPSTElement().value().illegalAccess().value()->getHash(),
				pst_fun->getHash(),
				base::strConcat(
					"The function origin PST element does not match the PST function for ",
					name.strView()
				)
			);
		};

		check_function_origin(base::StrID("a"));
		check_function_origin(base::StrID("b"));

		query::utils::withContextDo([&](query::Context& ctx) { hout.debugPrint(ctx, std::cout); });
	}

	void testScopeParentsAndDepth() {
		auto all_scopes = compiler::helios::getAllHeliosScopes();
		message(base::strConcat("Scope count: ", all_scopes.size()));
		for (auto scope: all_scopes) {
			auto depth = scopeDepth(scope);
			while (depth != 0) {
				scope = parent(scope).value();
				ASSERT_TRUE(depth > 0);
				ASSERT_EQUAL(depth - 1, scopeDepth(scope));
				depth = scopeDepth(scope);

				// it is just for cov mostly

				std::stringstream buffer;
				scope.debugPrintScopeAndParents(buffer);
			}
			assertTrue(parent(scope).empty(), "Scope at depth 0 can't have a parent");
		}
	}

	/**
	 * This checks for all symbols that if a given
	 * symbol `s` is in the scope `N`, then it is also in the
	 * output of QuerySymbolsInScope(N).
	 */
	void testScopeSymbolsConsistency() {
		auto all_symbols = compiler::helios::getAllHeliosSymbols();

		// this is quadratic in theory, if it ever get too slow,
		// we can optimize it with some maps.
		for (auto symbol: all_symbols) {
			auto maybe_scope = compiler::helios::maybeScope(symbol);
			if (maybe_scope.empty()) continue;
			auto scope            = maybe_scope.value();
			auto symbols_in_scope = query::entryPoint<compiler::helios::QuerySymbolsInScope>(scope);

			auto found = false;
			for (auto s: *symbols_in_scope) {
				if (s == symbol) {
					found = true;
					break;
				}
			}
			assertTrue(found, "Symbol was not fount in its scope");
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
