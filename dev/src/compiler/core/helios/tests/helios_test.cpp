#include <diagnostic_interactive/logger.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/binary_operator.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/pst_query/code_dependency.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/mutability.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios/utils/hout_walkers.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/expressions/casts.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/highlight_positions.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>
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
		TESTER_ADD_TEST(testTupleInterface);
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
		TESTER_ADD_TEST(testHoutWalkers);
		TESTER_ADD_TEST(testFunctions);
		TESTER_ADD_TEST(testStaticArrays);
		TESTER_ADD_TEST(testDynamicArrays);
		TESTER_ADD_TEST(testFunctionReturnTypeDeduction);
		TESTER_ADD_TEST(testFunctionReturnTypeCheckAndCoercion);
		TESTER_ADD_TEST(testTupleCoercion);
		TESTER_ADD_TEST(testMethodCalls);
		TESTER_ADD_TEST(testMangler);
		TESTER_ADD_TEST(testManglerSpecialMembers);
		TESTER_ADD_TEST(testManglerOperators);
		TESTER_ADD_TEST(testGlobalVariableExpressions);
		TESTER_ADD_TEST(testTypeOfConstAndVar);
		TESTER_ADD_TEST(testDebugPrint);
		TESTER_ADD_TEST(testStmtSpecifiers);
		TESTER_ADD_TEST(testOverloadResolution);
		TESTER_ADD_TEST(testCopyConstructors);
		TESTER_ADD_TEST(testCopyMoveOperators);
		TESTER_ADD_TEST(testDestructors);
		TESTER_ADD_TEST(testCastsHout);
		TESTER_ADD_TEST(testCastAs);
		TESTER_ADD_TEST(testPointers);
		TESTER_ADD_TEST(testTypeLifting);
		TESTER_ADD_TEST(testHoutElementsOrigin);
		TESTER_ADD_TEST(testAliases);
		TESTER_ADD_TEST(testBackendDependentCompTime);
		TESTER_ADD_TEST(testTemplates);
		TESTER_ADD_TEST(testOperatoriness);
		TESTER_ADD_TEST(testMethodOperatorResolution);

		// this is at the end
		// so we test all the scopes created in helios tests:
		TESTER_ADD_TEST(testScopeParentsAndDepth);
		TESTER_ADD_TEST(testScopeSymbolsConsistency);
	}

private:
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

	auto getSliceTypeNoContext(compiler::tsh::SymbolType<> element_type) {
		return std::any_cast<compiler::tsh::SliceAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return ctx.query<compiler::tsh::QuerySliceType>(element_type);
			})
		);
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

	static compiler::tsh::SymbolType<> stConst(const compiler::tsh::AbstractType abstract_type) {
		return st(abstract_type).withMutability(Immutable);
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

	static const compiler::helios::code::Expr* stripImplicitMove(
		const compiler::helios::code::Expr* expr
	) {
		using namespace compiler::helios;
		const auto* move = dynamic_cast<const code::MoveExpr*>(expr);
		if (move == nullptr || move->kind != code::MoveExpr::MoveKind::Implicit) return expr;
		return move->inner.get();
	}

	/**
	 * Get the value boxed by the `boxAlloc` call or nullptr on error.
	 */
	const compiler::helios::code::Expr* boxAllocArg(const compiler::helios::code::Expr* expr) {
		using namespace compiler::helios;
		const auto* call = dynamic_cast<const code::CallExpr*>(stripImplicitMove(expr));
		if (call == nullptr) return nullptr;
		const auto callee = getIdentifierExprSymID(call->callee.ref());
		if (!callee.has_value()) return nullptr;
		const auto builtin = isBuiltin(callee.value());
		if (!builtin.has_value() || builtin.value() != BuiltinKind::BoxAlloc) return nullptr;
		return stripImplicitMove(call->arguments.at(0).get());
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

		// Meta builtins (size_of / alignment_of) called through a VM comp-time function call.
		ASSERT_EQUAL(16, getConstValueAs<i64>("SIZE_AND_ALIGN_I64", root_scope));
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
				auto expected = stConst(i64_type);
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
				auto expected = stConst(i32_type);
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
		ASSERT_HAS_VALUE(first_class_info.destructor);
		ASSERT_NO_VALUE(first_class_info.base);
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
		ASSERT_NO_VALUE(second_class_info.destructor);
		ASSERT_HAS_VALUE(second_class_info.base);
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

		// @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		// that it is successful
		[[maybe_unused]]
		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module_id })
		          .valueOrPanic();
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

		// @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		// that it is successful
		[[maybe_unused]]
		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module_id })
		          .valueOrPanic();
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

	void testTupleInterface() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/tuples")));

		const auto tup = getChain("tup", root_scope).back();
		const auto tup_abstract_type
			= query::entryPoint<compiler::helios::QueryTypeOfSymbol>(tup)->valueOrThrow().getType();

		const auto h_interface = compiler::helios::HInterface::ofTypeInstance(tup_abstract_type);

		auto i32 = st(getIntegralTypeNoContext(32, Signed));
		auto f32 = st(getFloatTypeNoContext(32));
		query::utils::withContextDo([&](query::Context& ctx) {
			using compiler::helios::LookupResult;
			auto str = stConst(compiler::tsh::getCharSliceType(ctx));

			CRef<LookupResult> first_result
				= &h_interface.lookup(ctx, base::StrID("_1"))->valueOrPanic();
			ASSERT_TRUE(first_result->isSingle());
			auto first_symbol = first_result->leaves.at(0);
			ASSERT_EQUAL(kind(first_symbol), compiler::helios::SymbolKind::Field);
			ASSERT_EQUAL(
				ctx.query<compiler::helios::QueryTypeOfSymbol>(first_symbol)->valueOrThrow(), i32
			);

			CRef<LookupResult> second_result
				= &h_interface.lookup(ctx, base::StrID("_2"))->valueOrPanic();
			ASSERT_TRUE(second_result->isSingle());
			auto second_symbol = second_result->leaves.at(0);
			ASSERT_EQUAL(kind(second_symbol), compiler::helios::SymbolKind::Field);
			ASSERT_EQUAL(
				ctx.query<compiler::helios::QueryTypeOfSymbol>(second_symbol)->valueOrThrow(), f32
			);

			CRef<LookupResult> third_result
				= &h_interface.lookup(ctx, base::StrID("_3"))->valueOrPanic();
			ASSERT_TRUE(third_result->isSingle());
			auto third_symbol = third_result->leaves.at(0);
			ASSERT_EQUAL(kind(third_symbol), compiler::helios::SymbolKind::Field);
			ASSERT_EQUAL(
				ctx.query<compiler::helios::QueryTypeOfSymbol>(third_symbol)->valueOrThrow(), str
			);

			CRef<LookupResult> empty_result
				= &h_interface.lookup(ctx, base::StrID("_0"))->valueOrPanic();
			ASSERT_TRUE(empty_result->isEmpty());

			// Check that the module lowers to HOUT without throwing.
			[[maybe_unused]] const auto& hout
				= ctx.query<compiler::helios::QueryModuleHOUT>(module_id)->valueOrThrow();
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

		const auto int32_mut_symbol_type   = st(int32_type).withMutability(Mutable);
		const auto int32_immut_symbol_type = stConst(int32_type);

		ASSERT_EQUAL(int32_immut_symbol_type, getSymbolTypeOf("SimpleIntConst", root_scope));
		ASSERT_EQUAL(int32_immut_symbol_type, getSymbolTypeOf("SimpleIntLet", root_scope));
		ASSERT_EQUAL(int32_mut_symbol_type, getSymbolTypeOf("SimpleIntVar", root_scope));

		ASSERT_EQUAL(f32_type, getTypeOf("SimpleFloat", root_scope));
		ASSERT_EQUAL(bool_type, getTypeOf("SimpleBool", root_scope));

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

		// Lazily evaluated operands: the division by zero in the skipped part of the
		// expression must not be evaluated, otherwise these evaluations would fail.
		ASSERT_EQUAL(false, getConstValueAs<bool>("P1", root_scope));
		ASSERT_EQUAL(7, getConstValueAs<i32>("P2", root_scope));
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
		const auto a_member   = getChain("member_access", root_scope).back();
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

			// Build block expr
			std::vector<base::Box<Stmt>> block_statements;
			block_statements.emplace_back(makeBox<AssignmentStmt>(
				generatedOrigin(),
				makeBox<IdentifierExpr>(ctx, generatedOrigin(), a_member),
				makeBox<LiteralNumericExpr>(ctx, generatedOrigin(), 0)
			));
			block_statements.emplace_back(makeBox<ExprStmt>(
				generatedOrigin(),
				makeBox<VariantTypeConstructorExpr>(
					ctx, generatedOrigin(), std::move(variant_subtypes)
				)
			));

			auto mega_expr = makeBox<TernaryOperatorExpr>(
				ctx,
				generatedOrigin(),
				// Condition: ChainComparisonExpr (1 < 2 <= 3)
				makeBox<ChainComparisonExpr>(ctx, generatedOrigin(), std::move(comparisons)),
				// If true: SequenceExpr with nested expressions including CallExpr
				makeBox<SequenceExpr>(ctx, generatedOrigin(), std::move(sequence_exprs)),
				// If false: VariantTypeConstructorExpr(i64 | bool | string)
				makeBox<BlockExpr>(
					ctx,
					generatedOrigin(),
					makeBox<BlockStmt>(generatedOrigin(), CodeBlock{ std::move(block_statements) })
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

		ASSERT_EQUAL_PRINT(hout.functions.size(), 3);
		ASSERT_EQUAL_PRINT(hout.glob_data.size(), 3);

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

		ASSERT_EQUAL_PRINT(functions, 3);
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

		ASSERT_EQUAL_PRINT(functions, 1);
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

		std::ignore = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module);

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
		const auto f32_type    = getFloatTypeNoContext(32);
		const auto f32box_type = st(f32_type)
		                             .withReferenceKind(compiler::tsh::ReferenceKind::Box)
		                             .withMutability(Immutable);
		const auto vbox_type = query::entryPoint<compiler::helios::QueryTypeOfSymbol>(sym_vbox);
		ASSERT_EQUAL(f32box_type, vbox_type->valueOrThrow());

		auto              sym_vconst  = getChain("VCONST", root_scope).back();
		auto              tree_vconst = getExprOfConst(sym_vconst);
		std::stringstream out_vconst;
		tree_vconst->debugPrint(out_vconst);
		const auto bool_type       = compiler::tsh::getBoolType();
		const auto const_bool_type = stConst(bool_type);
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
		ASSERT_EQUAL(function->body->statements.size(), 7);

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
			std::ignore = i32_or_f32;  // < remove
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
				stripImplicitMove(var_stmt.initial_value.get())
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
				stripImplicitMove(var_stmt.initial_value.get())
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
				stripImplicitMove(ass_stmt.new_value_expr.get())
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
				stripImplicitMove(ass_stmt.new_value_expr.get())
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
				stripImplicitMove(var_stmt.initial_value.get())
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
				stripImplicitMove(var_stmt.initial_value.get())
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
				stripImplicitMove(var_stmt.initial_value.get())
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
			auto deref_expr = dynamic_cast<const compiler::helios::code::DerefExpr*>(
				stripImplicitMove(ret_stmt.value.get())
			);
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

			auto* boxed_value = boxAllocArg(stripImplicitMove(var_stmt->initial_value.get()));
			ASSERT_TRUE(boxed_value != nullptr);

			auto* literal_expr = dynamic_cast<const LiteralNumericExpr*>(boxed_value);
			ASSERT_TRUE(literal_expr != nullptr);

			auto var_type = var_stmt->type;
			ASSERT_EQUAL(var_type.getRefKind(), compiler::tsh::ReferenceKind::Box);
			ASSERT_EQUAL(var_type.getType().getKind(), compiler::tsh::Kind::Integral);
		}
		{
			// take_int_ref(&b_int);
			// `&` on `box i32` creates a `ref i32`
			ASSERT_TRUE(body.statements.size() > 2);
			auto* expr_stmt = dynamic_cast<const ExprStmt*>(body.statements[1].get());
			ASSERT_TRUE(expr_stmt != nullptr);
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
			ASSERT_TRUE(call_expr != nullptr);

			ASSERT_TRUE(call_expr->arguments.size() == 1);
			auto* ref_of = dynamic_cast<const RefOfExpr*>(call_expr->arguments[0].get());
			ASSERT_TRUE(ref_of != nullptr);
		}
		{
			// var x: i32 = b_point.x;
			ASSERT_TRUE(body.statements.size() > 5);
			auto* var_stmt = dynamic_cast<const VariableStmt*>(body.statements[4].get());
			ASSERT_TRUE(var_stmt != nullptr);

			auto* access_expr
				= dynamic_cast<const AccessExpr*>(stripImplicitMove(var_stmt->initial_value.get()));
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

			auto* deref_expr
				= dynamic_cast<const DerefExpr*>(stripImplicitMove(return_stmt->value.get()));
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
			auto* ref_of
				= dynamic_cast<const RefOfExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var ref_box_a: ref i32 = &box_a; (Box -> Ref)
		{
			const auto& var_stmt = get_var_stmt(4);
			ASSERT_EQUAL(var_stmt.type, ref_i32);
			auto* ref_of
				= dynamic_cast<const RefOfExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(ref_of != nullptr);
		}
		// var box_ref_a: box i32 = ref_a; (Ref -> Box)
		{
			const auto& var_stmt = get_var_stmt(5);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// This should create a copy. `box_alloc(DerefExpr(...))`
			auto* boxed_value = boxAllocArg(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(boxed_value != nullptr);
			auto* deref = dynamic_cast<const DerefExpr*>(boxed_value);
			ASSERT_TRUE(deref != nullptr);
		}
		// var box_box_a: box i32 = move box_a; (Box -> Box)
		{
			const auto& var_stmt = get_var_stmt(6);
			ASSERT_EQUAL(var_stmt.type, box_i32);
			// `box = box` needs an explicit `move`.
			auto* move_expr
				= dynamic_cast<const MoveExpr*>(stripImplicitMove(var_stmt.initial_value.get()));
			ASSERT_TRUE(move_expr != nullptr);
			auto* ident = dynamic_cast<const IdentifierExpr*>(move_expr->inner.get());
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
		auto call_expr = dynamic_cast<const compiler::helios::code::CallExpr*>(
			stripImplicitMove(variable->initial_value.get())
		);
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

	void testHoutWalkers() {
		auto [module, scope] = getModule(fs::File(path("test_modules/function_calls")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();
		ASSERT_EQUAL(4, hout.functions.size());

		using namespace compiler::helios::code;

		auto square_symbol = getChain("square", scope).back();
		auto foo1_symbol   = getChain("foo1", scope).back();

		// Look up the HOUT functions by name so the test does not depend on emission order.
		auto find_fun = [&](std::string_view target) -> const auto& {
			for (const auto& fun: hout.functions)
				if (fun->declaration->original_name.strView() == target) return *fun;
			CORE_PANIC(base::strConcat("function not found in HOUT unit: ", target));
		};

		const auto& square = find_fun("square");
		const auto& foo    = find_fun("foo");
		const auto& main   = find_fun("main");

		// foo() calls square() twice; the collected list is deduplicated to one entry.
		auto called_from_fun = collectCalledSymbolsFromHOUT(foo);
		ASSERT_EQUAL(1, called_from_fun.size());
		ASSERT_EQUAL(square_symbol, called_from_fun.at(0));

		// The last statement is `return square(a_squared + b);` — walking just that
		// expression tree should also find the call to square().
		const auto& return_stmt = dynamic_cast<const ReturnStmt&>(*foo.body->statements.back());
		auto        called_from_expr = collectCalledSymbolsFromHOUT(*return_stmt.value);
		ASSERT_EQUAL(1, called_from_expr.size());
		ASSERT_EQUAL(square_symbol, called_from_expr.at(0));

		// square() is a leaf: it calls no other functions.
		auto called_from_square = collectCalledSymbolsFromHOUT(square);
		ASSERT_EQUAL(0, called_from_square.size());

		// main() calls foo1() four times (with different argument styles); the collected list is
		// still deduplicated to a single entry.
		auto called_from_main = collectCalledSymbolsFromHOUT(main);
		ASSERT_EQUAL(1, called_from_main.size());
		ASSERT_EQUAL(foo1_symbol, called_from_main.at(0));

		// Walking a single call statement's expression tree finds just that callee.
		const auto& first_stmt        = dynamic_cast<const ExprStmt&>(*main.body->statements.at(0));
		auto        called_from_first = collectCalledSymbolsFromHOUT(*first_stmt.expr);
		ASSERT_EQUAL(1, called_from_first.size());
		ASSERT_EQUAL(foo1_symbol, called_from_first.at(0));
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
				auto ret_expr    = stripImplicitMove(stmt_casted->value.get());
				auto ctv
					= query::entryPoint<compiler::helios::QueryEvaluateHOUTExpression>({ ret_expr })
				          .valueOrThrow();
				ASSERT_EQUAL(1, ctv.get<compiler::numeric_value::NumericValue>()->get<i64>());
			}
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

			auto* default_val = dynamic_cast<const DefaultValueExpr*>(
				stripImplicitMove(var_decl.initial_value.get())
			);
			ASSERT_TRUE(default_val != nullptr);
		}
		{
			// l.push(1);
			auto& expr_stmt = dynamic_cast<const ExprStmt&>(*statements.at(1));
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt.expr.get());
			ASSERT_TRUE(call_expr != nullptr);
		}
		{
			// l.pop(1);
			auto& expr_stmt = dynamic_cast<const ExprStmt&>(*statements.at(2));
			auto* call_expr = dynamic_cast<const CallExpr*>(expr_stmt.expr.get());
			ASSERT_TRUE(call_expr != nullptr);
		}
		{
			// let l_len = l.length();
			auto& var_decl = dynamic_cast<const VariableStmt&>(*statements.at(3));
			auto* call_expr
				= dynamic_cast<const CallExpr*>(stripImplicitMove(var_decl.initial_value.get()));
			ASSERT_TRUE(call_expr != nullptr);
		}
		{
			// l[0] = 42;
			auto& assign_stmt = dynamic_cast<const AssignmentStmt&>(*statements.at(4));
			auto* index_expr  = dynamic_cast<const IndexExpr*>(assign_stmt.location_expr.get());
			ASSERT_TRUE(index_expr != nullptr);
		}
		{
			// let x = l[0];
			auto& var_decl = dynamic_cast<const VariableStmt&>(*statements.at(5));
			auto* index_expr
				= dynamic_cast<const IndexExpr*>(stripImplicitMove(var_decl.initial_value.get()));
			ASSERT_TRUE(index_expr != nullptr);
		}
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

		auto i32_type = getIntegralTypeNoContext(32, Signed);
		auto i64_type = getIntegralTypeNoContext(64, Signed);

		for (auto& function: hout.functions) {
			if (function->declaration->original_name == base::StrID("tuples")) {
				ASSERT_EQUAL(
					query::entryPoint<compiler::tsh::QueryTupleType>({ { st(i32_type),
				                                                         st(i64_type) } }),
					function->declaration->return_type.getType()
				);

				auto ret_stmt = function->body->statements.back().ref();
				auto ret_stmt_casted
					= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
				assertTrue(ret_stmt_casted != nullptr, "Return statement expected.");

				auto ret_type = ret_stmt_casted->value->expression_type.getType();
				ASSERT_EQUAL(function->declaration->return_type.getType(), ret_type);

				auto tuple_expr = dynamic_cast<const compiler::helios::code::TupleExpr*>(
					stripImplicitMove(ret_stmt_casted->value.get())
				);
				assertTrue(tuple_expr != nullptr, "Tuple expression expected.");

				int casts_found = 0;
				for (const auto& el: tuple_expr->elements) {
					auto cast_expr = dynamic_cast<const compiler::helios::code::CastExpr*>(&*el);
					if (cast_expr != nullptr) casts_found++;
				}
				assertEqual(1, casts_found, "Expected one cast to happen in tuple coercion.");

				continue;
			}

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
						stripImplicitMove(ret_stmt_casted->value.get())
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

			auto cast_expr = dynamic_cast<const compiler::helios::code::CastExpr*>(
				stripImplicitMove(ret_stmt_casted->value.get())
			);
			assertTrue(cast_expr != nullptr, "Cast expression expected.");
		}
	}

	/**
	 * @brief Checks that tuples are coerced element by element, and that tuple literals are coerced
	 * in place.
	 *
	 * A tuple written as a literal keeps the shape of its element expressions, so the coercion is
	 * applied directly to them and the resulting `TupleExpr` holds no `ReusableExpr`. That matters
	 * for elements lifted to a `type`: lifting only works on the original expression, not on a
	 * field read out of a materialised tuple. A tuple that is already a value has to be
	 * materialised, so its elements are read back out of a `ReusableExpr` instead.
	 */
	void testTupleCoercion() {
		auto [module, scope] = getModule(fs::File(path("test_modules/tuple_coercion")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		// Returns the coerced tuple of the function's `return` statement.
		auto returned_tuple = [&](const auto& function) {
			auto  ret_stmt = function->body->statements.back().ref();
			auto* ret_stmt_casted
				= dynamic_cast<const compiler::helios::code::ReturnStmt*>(&*ret_stmt);
			assertTrue(ret_stmt_casted != nullptr, "Return statement expected.");

			ASSERT_EQUAL(
				function->declaration->return_type.getType(),
				ret_stmt_casted->value->expression_type.getType()
			);

			auto* tuple_expr = dynamic_cast<const compiler::helios::code::TupleExpr*>(
				ret_stmt_casted->value.get()
			);
			assertTrue(tuple_expr != nullptr, "Tuple expression expected.");
			return tuple_expr;
		};

		// Whether any element of the tuple was read out of a materialised source tuple.
		auto uses_reusable_source = [](const auto* tuple_expr) {
			for (const auto& element: tuple_expr->elements) {
				const compiler::helios::code::Expr* current = &*element;
				while (auto* cast = dynamic_cast<const compiler::helios::code::CastExpr*>(current))
					current = cast->source_expr.get();

				if (dynamic_cast<const compiler::helios::code::AccessExpr*>(current) != nullptr)
					return true;
			}
			return false;
		};

		bool literal_checked       = false;
		bool parenthesised_checked = false;
		bool lift_checked          = false;
		bool bool_checked          = false;
		bool materialised_checked  = false;

		for (auto& function: hout.functions) {
			const auto name = function->declaration->original_name;

			if (name == base::StrID("literal") || name == base::StrID("parenthesised")) {
				auto* tuple_expr = returned_tuple(function);
				ASSERT_EQUAL(tuple_expr->elements.size(), 2u);

				int casts_found = 0;
				for (const auto& element: tuple_expr->elements)
					if (dynamic_cast<const compiler::helios::code::CastExpr*>(&*element) != nullptr)
						casts_found++;
				assertEqual(1, casts_found, "Only the widened element should be cast.");

				assertTrue(
					!uses_reusable_source(tuple_expr),
					"A tuple literal should be coerced in place, without being materialised."
				);

				(name == base::StrID("literal") ? literal_checked : parenthesised_checked) = true;
			} else if (name == base::StrID("lift_literal")) {
				auto* tuple_expr = returned_tuple(function);
				ASSERT_EQUAL(tuple_expr->elements.size(), 2u);

				auto* lift = dynamic_cast<const compiler::helios::code::LiftToTypeExpr*>(
					&*tuple_expr->elements[0]
				);
				assertTrue(lift != nullptr, "The unit element should be lifted to a type.");

				lift_checked = true;
			} else if (name == base::StrID("to_bool")) {
				auto* tuple_expr = returned_tuple(function);
				ASSERT_EQUAL(tuple_expr->elements.size(), 2u);

				auto* zero_check = dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(
					&*tuple_expr->elements[0]
				);
				assertTrue(zero_check != nullptr, "The bool element should be a zero-check.");

				bool_checked = true;
			} else if (name == base::StrID("materialised")) {
				auto* tuple_expr = returned_tuple(function);
				ASSERT_EQUAL(tuple_expr->elements.size(), 2u);

				assertTrue(
					uses_reusable_source(tuple_expr),
					"A tuple value should be materialised and its elements read back out of it."
				);

				materialised_checked = true;
			}
		}

		assertTrue(literal_checked, "Function `literal` was not found.");
		assertTrue(parenthesised_checked, "Function `parenthesised` was not found.");
		assertTrue(lift_checked, "Function `lift_literal` was not found.");
		assertTrue(bool_checked, "Function `to_bool` was not found.");
		assertTrue(materialised_checked, "Function `materialised` was not found.");
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
		ASSERT_EQUAL(6, point_class_info.methods.size());

		for (auto& method: point_class_info.methods) {
			auto method_hout
				= query::entryPoint<compiler::helios::QueryCodeOfFun>({ method })->valueOrPanic();
			ASSERT_EQUAL(
				refst(point_class_abstract_type), method_hout.declaration->parameters.at(0).type
			);
		}

		// @note: #973 when QueryModuleHOUTRecursively returns QResult, add assertion
		// that it is successful
		[[maybe_unused]]
		std::vector<CRef<compiler::helios::HOUTUnit>> units
			= query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>({ module })
		          .valueOrPanic();
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

	void testManglerOperators() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/mangling_operators")));

		auto mangle = [&](compiler::helios::SymID sym) {
			return query::entryPoint<compiler::helios::mangler::QueryMangledSymbol>(
				{ .symbol_key = sym,
			      .kind       = compiler::helios::mangler::ManglingSymbolKind::Standard,
			      .mangling_scheme_version = 0,
			      .additional_metadata     = std::nullopt }
			);
		};

		auto infix_free   = mangle(getChain("+*", root_scope).back());
		auto prefix_free  = mangle(getChain("-*", root_scope).back());
		auto unicode_free = mangle(getChain("+×", root_scope).back());

		auto foo_class = getChain("Foo", root_scope).back();
		auto foo_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(foo_class)->valueOrThrow();
		ASSERT_EQUAL(2, foo_class_info.methods.size());

		auto find_method = [&](std::string_view name) {
			for (const auto& method: foo_class_info.methods)
				if (compiler::helios::name(method) == base::StrID(name)) return method;
			fail(base::strConcat("Method ", name, " not found"));
			return foo_class_info.methods.at(0);
		};

		auto infix_method  = mangle(find_method("+*"));
		auto prefix_method = mangle(find_method("-*"));

		ASSERT_EQUAL("_Q_M18mangling_operatorsGOi4plmlFi64i64i64E1a1bE", infix_free.str());
		ASSERT_EQUAL("_Q_M18mangling_operatorsGOp4mimlFi64i64E1aE", prefix_free.str());
		ASSERT_EQUAL("_Q_M18mangling_operatorsGOi6plxd7_Fi64i64i64E1a1bE", unicode_free.str());
		ASSERT_EQUAL(
			"_Q_M18mangling_operatorsN3FooOi4plmlEFi64R_Q_CM18mangling_operatorsG3Fooi64E4self1aE",
			infix_method.str()
		);
		ASSERT_EQUAL(
			"_Q_M18mangling_operatorsN3FooOp4mimlEFi64R_Q_CM18mangling_operatorsG3FooE4selfE",
			prefix_method.str()
		);

		// Suffix has no declaration syntax yet (see testOperatoriness) -- nothing to mangle here
		// until fixity keywords exist. Once they do, add e.g.:
		// ASSERT_EQUAL("...", mangle(.../* a suffix-declared operator */).str());
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

		const auto str_type
			= base::anyCast<compiler::tsh::SliceAbstractType>(query::utils::withContextCompute(
				[&](query::Context& ctx) { return compiler::tsh::getCharSliceType(ctx); }
			));

		const auto tuple_ii_type = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { st(int32_type), st(int32_type) } }
		);
		const auto tuple_si_type = query::entryPoint<compiler::tsh::QueryTupleType>(
			{ { stConst(str_type), st(int32_type) } }
		);

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
				if (unlocked->getSpecifier().unlock(ctx)->unwrap() == keyword) return true;
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
				if (unlocked->getSpecifier().unlock(ctx)->unwrap() != expected_order[i])
					return false;
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
				ASSERT_HAS_VALUE(c_abi.library);
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

		// Test symbols nested in a namespace, in both nesting orders
		auto c_nested_in_namespace
			= getChain("c_block_namespace.cNestedInNamespace", root_scope).back();
		auto c_reverse_nested = getChain("reverse_namespace.cReverseNested", root_scope).back();

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

			// Test namespaces nested in an extern("C") block and vice versa: the extern
			// specifier propagates through namespaces in both nesting orders
			{
				for (auto symbol: { c_nested_in_namespace, c_reverse_nested }) {
					auto specifiers = ctx.query<compiler::helios::QuerySpecifiersOfSymbol>(symbol);
					ASSERT_TRUE(has_specifier(ctx, *specifiers, pst::Keyword::Extern));
					test_c_abi_with_library(ctx, symbol, {});
				}
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

	void testCopyConstructors() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/copy_constructors")));

		auto trivial_sym = getChain("Trivial", root_scope).back();
		auto has_box_sym = getChain("HasBox", root_scope).back();
		auto holds_sym   = getChain("HoldsNonTrivial", root_scope).back();
		auto mega_sym    = getChain("FinalBoss", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type = [&](SymID sym_id) {
				return ctx.query<QueryTypeFromDefinition>(sym_id)->valueOrThrow().getType();
			};

			// Classes, tuples and static arrays are copied by a single `create_aggregate` that the
			// body returns, with one value per copied element.
			auto returned_aggregate = [](const HOUTFunction& cctor) -> const CreateAggregateExpr& {
				auto ret = dynamic_cast<const ReturnStmt*>(cctor.body->statements.back().get());
				CORE_ASSERT(ret != nullptr, "The copy constructor's body must end with a return.");
				auto aggregate
					= dynamic_cast<const CreateAggregateExpr*>(stripImplicitMove(ret->value.get()));
				CORE_ASSERT(aggregate != nullptr, "The copy constructor must return an aggregate.");
				return *aggregate;
			};

			// The symbol of the copy constructor invoked by a copy-ctor-call expression.
			auto callee_of = [&](const Expr* expr) -> SymID {
				auto call = dynamic_cast<const CallExpr*>(stripImplicitMove(expr));
				ASSERT_TRUE(call != nullptr);
				return getIdentifierExprSymID(call->callee.ref()).value();
			};

			auto assert_generated_copy = [&](const Expr* expr) {
				const auto* ctor = std::get_if<Constructor>(&getSymRef(callee_of(expr))->other);
				ASSERT_TRUE(ctor != nullptr);
				ASSERT_EQUAL(ctor->kind, Constructor::Kind::Copy);
			};

			// For a trivially-copyable class, the copy constructor copies each field with byte
			// copy, so each value of the aggregate is a field access, not a copy-ctor call.
			{
				auto        trivial_type = get_class_type(trivial_sym);
				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(trivial_type)->valueOrThrow();

				// `(const ref Trivial) -> Trivial`.
				ASSERT_EQUAL_PRINT(1, cctor.declaration->parameters.size());
				const auto param_type = cctor.declaration->parameters.at(0).type;
				ASSERT_EQUAL(compiler::tsh::ReferenceKind::Ref, param_type.getRefKind());
				ASSERT_EQUAL(compiler::tsh::Mutability::Immutable, param_type.getMutability());
				ASSERT_EQUAL(trivial_type, param_type.getType());
				ASSERT_EQUAL(trivial_type, cctor.declaration->return_type.getType());

				// return create_aggregate(Trivial) { (*source).a, (*source).b };
				ASSERT_EQUAL_PRINT(1, cctor.body->statements.size());
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(1).get())) != nullptr
				);
			}

			// For a class holding a non-trivially-copyable field, the copy constructor copies the
			// field by calling that field type's copy constructor.
			{
				auto        holds_type = get_class_type(holds_sym);
				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(holds_type)->valueOrThrow();

				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				assert_generated_copy(stripImplicitMove(values.at(0).get()));
			}

			// A tuple is copied element-by-element just like a class. Trivial elements are
			// byte-copied, non-trivially-copyable elements are copied with their own copy constructor.
			{
				auto i32_type = compiler::tsh::getIntegralType(
					ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed
				);
				auto has_box_st = st(get_class_type(has_box_sym));
				auto tuple_type
					= ctx.query<compiler::tsh::QueryTupleType>({ { st(i32_type), has_box_st } });

				const auto& cctor
					= ctx.query<QueryDefaultCopyConstructor>(tuple_type)->valueOrThrow();

				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());

				// First element is trivially copyable, the second one requires a copy ctor.
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				assert_generated_copy(stripImplicitMove(values.at(1).get()));
			}

			auto dump_cctor = [&](std::string_view            label,
			                      compiler::tsh::AbstractType type) -> const HOUTFunction& {
				const auto& cctor = ctx.query<QueryDefaultCopyConstructor>(type)->valueOrThrow();
				std::cerr << "\n===== copy constructor: " << label << " =====\n";
				for (const auto& stmt: cctor.body->statements) {
					stmt->debugPrint(std::cerr, 1);
					std::cerr << "\n";
				}
				return cctor;
			};

			const auto  final_boss_type  = get_class_type(mega_sym);
			const auto& final_boss_cctor = dump_cctor("FinalBoss", final_boss_type);

			// The value of the aggregate that copies field `field_name`. The values are in field
			// declaration order, one per field.
			auto rhs_of = [&](std::string_view field_name) -> const Expr* {
				const auto& values = returned_aggregate(final_boss_cctor).values;
				usize       index  = 0;
				for (const auto& field: final_boss_type.getInterface(ctx)->getFieldsView()) {
					if (compiler::helios::name(field.getSymbol()).strView() == field_name)
						return values.at(index).get();
					index++;
				}
				CORE_PANIC("no value found for field");
			};

			// Trivially-copyable fields are byte-copied. The RHS is should be a plain field access.
			for (std::string_view trivial_field:
			     { "i", "f", "flag", "r", "p", "c", "m", "trivial_arr", "trivial_tup" })
				ASSERT_TRUE(dynamic_cast<const AccessExpr*>(rhs_of(trivial_field)) != nullptr);

			// `box i32` - deep copy of a trivial pointee -> box_alloc(*source.boxed_prim).
			{
				auto boxed = boxAllocArg(rhs_of("boxed_prim"));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_TRUE(dynamic_cast<const DerefExpr*>(boxed) != nullptr);
			}

			// `box HasBox` - deep copy of a non-trivial pointee -> box_alloc(HasBox.__copy(...)).
			{
				auto boxed = boxAllocArg(rhs_of("boxed_class"));
				ASSERT_TRUE(boxed != nullptr);
				assert_generated_copy(boxed);
			}

			// Non-trivial aggregates call a copy constructor.
			for (std::string_view aggregate_field:
			     { "nontrivial_arr", "nontrivial_tup", "prim_list", "class_list", "nested_default" })
				assert_generated_copy(rhs_of(aggregate_field));

			// A field whose class defines a user copy constructor calls the user code, not a
			// generated one.
			ASSERT_TRUE(v_matches(
				getSymRef(callee_of(rhs_of("nested_user")))->other, PstImplementedSemantics
			));

			// `box UserCopied` - deep copy whose inner pointee copy runs the user constructor.
			{
				auto boxed = boxAllocArg(rhs_of("deep"));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_TRUE(v_matches(getSymRef(callee_of(boxed))->other, PstImplementedSemantics));
			}

			auto field_abstract_type = [&](std::string_view field_name) {
				return final_boss_type.getInterface(ctx)
				    ->getElementsWithName(base::StrID(field_name))
				    .back()
				    .getType(ctx)
				    .getType();
			};

			// HasBox -> box_alloc(*source.boxed).
			{
				const auto& cctor  = dump_cctor("HasBox", get_class_type(has_box_sym));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				auto boxed = boxAllocArg(stripImplicitMove(values.at(0).get()));
				ASSERT_TRUE(boxed != nullptr);
				ASSERT_TRUE(dynamic_cast<const DerefExpr*>(boxed) != nullptr);
			}

			// HoldsNonTrivial - copies its `HasBox` field with a copy-ctor call.
			{
				const auto& cctor  = dump_cctor("HoldsNonTrivial", get_class_type(holds_sym));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(1, values.size());
				assert_generated_copy(stripImplicitMove(values.at(0).get()));
			}

			// Static array - one value copying the element the index variable points at, with the
			// per-element statements advancing that index.
			{
				const auto& cctor = dump_cctor("HasBox[3]", field_abstract_type("nontrivial_arr"));
				const auto& stmts = cctor.body->statements;

				// var __i = 0;
				// return create_aggregate(HasBox[3]) [ (*source)[__i].__copy() ]
				//     per_element { __i = __i + 1; };
				ASSERT_EQUAL_PRINT(2, stmts.size());
				ASSERT_TRUE(dynamic_cast<const VariableStmt*>(stmts.front().get()) != nullptr);

				const auto& aggregate = returned_aggregate(cctor);
				ASSERT_EQUAL_PRINT(1, aggregate.values.size());
				assert_generated_copy(stripImplicitMove(aggregate.values.at(0).get()));

				ASSERT_TRUE(aggregate.per_element_body.has_value());
				const auto& per_element = (*aggregate.per_element_body)->statements;
				ASSERT_EQUAL_PRINT(1, per_element.size());
				ASSERT_TRUE(dynamic_cast<const AssignmentStmt*>(per_element.at(0).get()) != nullptr);
			}

			// Tuple with a non-trivial element - element 0 byte-copied, element 1 via a copy ctor.
			{
				const auto& cctor
					= dump_cctor("(i32, HasBox)", field_abstract_type("nontrivial_tup"));
				const auto& values = returned_aggregate(cctor).values;
				ASSERT_EQUAL_PRINT(2, values.size());
				ASSERT_TRUE(
					dynamic_cast<const AccessExpr*>(stripImplicitMove(values.at(0).get())) != nullptr
				);
				assert_generated_copy(stripImplicitMove(values.at(1).get()));
			}

			// List of a trivial element.
			{
				const auto& cctor = dump_cctor("List[i32]", field_abstract_type("prim_list"));
				const auto& stmts = cctor.body->statements;
				ASSERT_EQUAL_PRINT(4, stmts.size());
				ASSERT_TRUE(dynamic_cast<const WhileStmt*>(stmts.at(2).get()) != nullptr);
			}

			// List of a non-trivial element - each pushed element is a copy-ctor call.
			{
				const auto& cctor = dump_cctor("List[HasBox]", field_abstract_type("class_list"));
				const auto& stmts = cctor.body->statements;
				ASSERT_EQUAL_PRINT(4, stmts.size());
				auto while_stmt = dynamic_cast<const WhileStmt*>(stmts.at(2).get());
				ASSERT_TRUE(while_stmt != nullptr);
				ASSERT_TRUE(!while_stmt->body.statements.empty());
				auto push_stmt
					= dynamic_cast<const ExprStmt*>(while_stmt->body.statements.at(0).get());
				ASSERT_TRUE(push_stmt != nullptr);
				auto push = dynamic_cast<const ListPushExpr*>(push_stmt->expr.get());
				ASSERT_TRUE(push != nullptr);
				assert_generated_copy(stripImplicitMove(push->element.get()));
			}
		});
	}

	void testCopyMoveOperators() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;

		auto [module, _] = getModule(fs::File(path("test_modules/copy_move_ops")));

		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		const HOUTFunction* fn = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "usesOps") fn = &*f;
		ASSERT_TRUE(fn != nullptr);

		// var a = W(1);
		// var b = copy a;
		// var c = move a;
		// return c.x;
		const auto& stmts = fn->body->statements;
		ASSERT_EQUAL_PRINT(4, stmts.size());

		// `copy a` lowers to a copy ctor call.
		const auto* b_var = dynamic_cast<const VariableStmt*>(stmts.at(1).get());
		ASSERT_TRUE(b_var != nullptr);
		ASSERT_TRUE(
			dynamic_cast<const CallExpr*>(stripImplicitMove(b_var->initial_value.get())) != nullptr
		);

		// `move a` lowers to a MoveExpr.
		const auto* c_var = dynamic_cast<const VariableStmt*>(stmts.at(2).get());
		ASSERT_TRUE(c_var != nullptr);
		const auto* c_move
			= dynamic_cast<const MoveExpr*>(stripImplicitMove(c_var->initial_value.get()));
		ASSERT_TRUE(c_move != nullptr);
		ASSERT_EQUAL(MoveExpr::MoveKind::Explicit, c_move->kind);

		// `return w` should move the owned local implicitly out without a `move` keyword
		const HOUTFunction* returns_local = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "returnsLocal") returns_local = &*f;
		ASSERT_TRUE(returns_local != nullptr);

		const auto& return_stmts = returns_local->body->statements;
		const auto* ret_stmt     = dynamic_cast<const ReturnStmt*>(return_stmts.back().get());
		ASSERT_TRUE(ret_stmt != nullptr);

		const auto* implicit_move = dynamic_cast<const MoveExpr*>(ret_stmt->value.get());
		ASSERT_TRUE(implicit_move != nullptr);
		ASSERT_EQUAL(MoveExpr::MoveKind::Implicit, implicit_move->kind);
		ASSERT_TRUE(dynamic_cast<const IdentifierExpr*>(implicit_move->inner.get()) != nullptr);

		const HOUTFunction* through_ref = nullptr;
		for (auto& f: hout.functions)
			if (f->declaration->original_name == "copiesThroughRef") through_ref = &*f;
		ASSERT_TRUE(through_ref != nullptr);

		// var b = copy r;   (r: ref W)
		// var c = copy bx;  (bx: box W)
		// Both should lower to a copy ctor call.
		const auto& ref_stmts = through_ref->body->statements;
		ASSERT_EQUAL_PRINT(3, ref_stmts.size());

		for (const usize i: { 0uz, 1uz }) {
			const auto* var_stmt = dynamic_cast<const VariableStmt*>(ref_stmts.at(i).get());
			ASSERT_TRUE(var_stmt != nullptr);
			ASSERT_TRUE(
				dynamic_cast<const CallExpr*>(stripImplicitMove(var_stmt->initial_value.get()))
				!= nullptr
			);
		}
	}

	void testDestructors() {
		using namespace compiler::helios;
		using namespace compiler::helios::code;
		using namespace compiler::helios::defgen;

		auto [module, root_scope] = getModule(fs::File(path("test_modules/destructors")));

		auto trivial_sym      = getChain("Trivial", root_scope).back();
		auto has_box_sym      = getChain("HasBox", root_scope).back();
		auto user_sym         = getChain("UserDestroyed", root_scope).back();
		auto holds_sym        = getChain("HoldsNonTrivial", root_scope).back();
		auto user_members_sym = getChain("UserAndMembers", root_scope).back();
		auto boss_sym         = getChain("FinalBoss", root_scope).back();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto get_class_type = [&](SymID sym_id) {
				return ctx.query<QueryTypeFromDefinition>(sym_id)->valueOrThrow().getType();
			};

			auto is_builtin_call = [&](const Stmt* stmt, BuiltinKind kind) -> bool {
				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmt);
				if (expr_stmt == nullptr) return false;
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				if (call == nullptr) return false;
				auto callee = getIdentifierExprSymID(call->callee.ref());
				if (!callee.has_value()) return false;
				auto builtin = isBuiltin(callee.value());
				return builtin.has_value() && builtin.value() == kind;
			};

			auto is_method_call = [&](const Stmt* stmt, Method::Kind kind) -> bool {
				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmt);
				if (expr_stmt == nullptr) return false;
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				if (call == nullptr) return false;
				auto callee = getIdentifierExprSymID(call->callee.ref());
				if (!callee.has_value()) return false;
				const auto* method = std::get_if<Method>(&getSymRef(callee.value())->other);
				return method != nullptr && method->kind == kind;
			};

			// Returns the callee symbol of a statement of the form `f(...);`, if any.
			auto call_callee = [&](const Stmt* stmt) -> base::Optional<SymID> {
				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmt);
				if (expr_stmt == nullptr) return {};
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				if (call == nullptr) return {};
				return getIdentifierExprSymID(call->callee.ref());
			};

			auto is_templated_builtin_call
				= [&](const Stmt* stmt, BuiltinTemplatedSymbol::Kind kind) -> bool {
				auto callee = call_callee(stmt);
				if (!callee.has_value()) return false;
				const auto* templated
					= std::get_if<BuiltinTemplatedSymbol>(&getSymRef(callee.value())->other);
				return templated != nullptr && templated->kind == kind;
			};

			// A trivially-destructible class has an empty destructor and a no-op destructor.
			{
				const auto  type = get_class_type(trivial_sym);
				const auto& dtor = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();

				// `(ref mut Trivial) -> ()`.
				ASSERT_EQUAL_PRINT(1, dtor.declaration->parameters.size());
				const auto self_type = dtor.declaration->parameters.at(0).type;
				ASSERT_EQUAL(compiler::tsh::ReferenceKind::Ref, self_type.getRefKind());
				ASSERT_EQUAL(compiler::tsh::Mutability::Mutable, self_type.getMutability());
				ASSERT_EQUAL(type, self_type.getType());
				ASSERT_EQUAL(
					compiler::tsh::Kind::Unit, dtor.declaration->return_type.getType().getKind()
				);

				ASSERT_TRUE(dtor.body->statements.empty());
				ASSERT_TRUE(type.hasNoOpDestructor(ctx));
			}

			// A class owning a `box i32` destroys the box by calling its `box_destructor`. That
			// builtin's own body frees the storage with `box_free`; the pointee is trivial, so
			// there is no pointee destruction, only the free.
			{
				const auto type = get_class_type(has_box_sym);
				ASSERT_TRUE(!type.hasNoOpDestructor(ctx));

				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_templated_builtin_call(
					stmts.at(0).get(), BuiltinTemplatedSymbol::Kind::BoxDestructor
				));

				// The box_destructor's own body only frees the (trivial) box storage.
				const auto box_dtor_sym = call_callee(stmts.at(0).get());
				ASSERT_HAS_VALUE(box_dtor_sym);
				const auto& box_dtor
					= ctx.query<QueryCodeOfFun>(box_dtor_sym.value())->valueOrThrow();
				ASSERT_EQUAL_PRINT(1, box_dtor.body->statements.size());
				ASSERT_TRUE(
					is_builtin_call(box_dtor.body->statements.at(0).get(), BuiltinKind::BoxFree)
				);
			}

			// A class holding a non-trivially-destructible member destroys it via that member's own
			// destructor.
			{
				const auto  type  = get_class_type(holds_sym);
				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_method_call(stmts.at(0).get(), Method::Kind::DefaultDestructor));
			}

			// A class that declares a user destructor should call the user code first.
			{
				const auto type = get_class_type(user_sym);
				ASSERT_TRUE(!type.hasNoOpDestructor(ctx));

				const auto user_dtor = userDestructorOf(ctx, user_sym);
				ASSERT_HAS_VALUE(user_dtor);
				ASSERT_TRUE(isUserDefinedDestructor(ctx, user_dtor.value()));
				ctx.query<QueryCodeOfFun>(user_dtor.value())->valueOrThrow();

				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(1, stmts.size());

				auto expr_stmt = dynamic_cast<const ExprStmt*>(stmts.at(0).get());
				ASSERT_TRUE(expr_stmt != nullptr);
				auto call = dynamic_cast<const CallExpr*>(expr_stmt->expr.get());
				ASSERT_TRUE(call != nullptr);
				ASSERT_EQUAL(user_dtor.value(), getIdentifierExprSymID(call->callee.ref()).value());
			}

			// A class with a user destructor and non-trivial members should call the user code
			// first, then destroy the members in reverse order.
			{
				const auto  type  = get_class_type(user_members_sym);
				const auto& dtor  = ctx.query<QueryDefaultDestructor>(type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(3, stmts.size());

				// [0] user destructor call.
				const auto user_dtor = userDestructorOf(ctx, user_members_sym).value();
				auto       user_call = dynamic_cast<const CallExpr*>(
                    dynamic_cast<const ExprStmt*>(stmts.at(0).get())->expr.get()
                );
				ASSERT_TRUE(user_call != nullptr);
				ASSERT_EQUAL(user_dtor, getIdentifierExprSymID(user_call->callee.ref()).value());

				// [1] `second` (box i32) destroyed via its box_destructor
				ASSERT_TRUE(is_templated_builtin_call(
					stmts.at(1).get(), BuiltinTemplatedSymbol::Kind::BoxDestructor
				));
				// [2] `first` (HasBox) destroyed
				ASSERT_TRUE(is_method_call(stmts.at(2).get(), Method::Kind::DefaultDestructor));
			}

			auto field_abstract_type = [&](std::string_view field_name) {
				return get_class_type(boss_sym)
				    .getInterface(ctx)
				    ->getElementsWithName(base::StrID(field_name))
				    .back()
				    .getType(ctx)
				    .getType();
			};

			// Static array of a non-trivial element - a destructor loop.
			{
				const auto  arr_type = field_abstract_type("nontrivial_arr");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(arr_type)->valueOrThrow();
				const auto& stmts    = dtor.body->statements;
				// var __i = 0; while (__i < 3) { ... }
				ASSERT_EQUAL_PRINT(2, stmts.size());
				ASSERT_TRUE(dynamic_cast<const WhileStmt*>(stmts.at(1).get()) != nullptr);
			}

			// Static array of a trivial element - empty destructor.
			{
				const auto  arr_type = field_abstract_type("trivial_arr");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(arr_type)->valueOrThrow();
				ASSERT_TRUE(dtor.body->statements.empty());
			}

			// List of a non-trivial element - a destruction loop over the elements, then the
			// backing buffer is released with a `list_free` builtin call.
			{
				const auto  list_type = field_abstract_type("class_list");
				const auto& dtor  = ctx.query<QueryDefaultDestructor>(list_type)->valueOrThrow();
				const auto& stmts = dtor.body->statements;
				ASSERT_EQUAL_PRINT(3, stmts.size());
				ASSERT_TRUE(dynamic_cast<const WhileStmt*>(stmts.at(1).get()) != nullptr);
				ASSERT_TRUE(is_templated_builtin_call(
					stmts.at(2).get(), BuiltinTemplatedSymbol::Kind::ListFree
				));
			}

			// Tuple with a non-trivial element - destroys that element via its destructor.
			{
				const auto  tup_type = field_abstract_type("nontrivial_tup");
				const auto& dtor     = ctx.query<QueryDefaultDestructor>(tup_type)->valueOrThrow();
				const auto& stmts    = dtor.body->statements;
				// Only the `HasBox` needs destruction.
				ASSERT_EQUAL_PRINT(1, stmts.size());
				ASSERT_TRUE(is_method_call(stmts.at(0).get(), Method::Kind::DefaultDestructor));
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

	/**
	 * Unit tests of `castAs` (the whole `as` operator handling) called directly on synthesized
	 * HOUT expressions, without going through the PST -> HOUT pipeline. A cast that the implicit
	 * coercion covers must reuse the coercion result (a comparison for the `-> bool` case), while
	 * the remaining scalar conversions become a `CastExpr`.
	 */
	void testCastAs() {
		using namespace compiler::helios::code;
		using compiler::tsh::AbstractType;
		auto [module_id, _] = getModule(fs::File(path("test_modules/casts")));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto pst
				= getFilePST(ctx, ctx.query<compiler::frontend::QueryMainSourceFile>(module_id));
			// Any binary operator node of the module does: `castAs` only reads the origin and the
			// position for the diagnostics out of it.
			auto as_stmt
				= pst::viewAllSubTreeElementsFilter<pst::expr::BinaryOperator>(pst->getRootElement())
			          .at(0)
			          .unlock(ctx);

			const auto i32_t  = compiler::tsh::getIntegralType(ctx, 32, Signed);
			const auto i64_t  = compiler::tsh::getIntegralType(ctx, 64, Signed);
			const auto f64_t  = compiler::tsh::getFloatType(ctx, 64);
			const auto bool_t = compiler::tsh::getBoolType();

			auto literal = [&](const AbstractType type) -> Box<Expr> {
				if (type.getKind() == compiler::tsh::Kind::Bool)
					return makeBox<LiteralBoolExpr>(ctx, generatedOrigin(), true);
				return makeBox<LiteralNumericExpr>(
					ctx,
					generatedOrigin(),
					compiler::numeric_value::NumericValue::createOfType(type, 1).expect(
						"Failed to create the source literal of the cast"
					)
				);
			};
			auto cast = [&](const AbstractType from, const AbstractType to) {
				return castAs(ctx, literal(from), st(to), as_stmt);
			};
			// Asserts that the cast produced a `CastExpr` onto `to`.
			auto assert_cast_to = [&](const AbstractType from, const AbstractType to) {
				auto result   = cast(from, to);
				auto cast_ptr = dynamic_cast<const CastExpr*>(result.get());
				ASSERT_TRUE(cast_ptr != nullptr);
				ASSERT_EQUAL(to, cast_ptr->target_type.getType());
			};

			// Implicit widening, explicit narrowing and both directions between int and float are
			// all plain conversions.
			assert_cast_to(i32_t, i64_t);
			assert_cast_to(i64_t, i32_t);
			assert_cast_to(i64_t, f64_t);
			assert_cast_to(f64_t, i32_t);

			// `bool` is one bit wide, so a numeric source must not be truncated into it. The
			// coercion turns it into a `!= 0` comparison instead of a `CastExpr`.
			auto to_bool = cast(i64_t, bool_t);
			auto cmp_ptr = dynamic_cast<const BinaryOperatorExpr*>(to_bool.get());
			ASSERT_TRUE(cmp_ptr != nullptr);
			ASSERT_EQUAL(BuiltinBinary::IntegerNeq, cmp_ptr->operation);
			ASSERT_EQUAL(bool_t, to_bool->expression_type.getType());

			// The other direction is a normal widening of the 0/1 value.
			assert_cast_to(bool_t, i64_t);

			// A cast to the source's own type is a no-op: the value is returned untouched.
			auto same = cast(i64_t, i64_t);
			ASSERT_TRUE(dynamic_cast<const LiteralNumericExpr*>(same.get()) != nullptr);
		});
	}

	void testPointers() {
		auto [module, top_scope] = getModule(fs::File(path("test_modules/pointers")));
		auto& hout
			= query::entryPoint<compiler::helios::QueryTopLevelEntities>(module)->valueOrPanic();

		auto test_pointers_sym = getChain("test_pointers", top_scope).back();
		auto body_scope        = getFunctionBodyScope(test_pointers_sym);

		auto i32_type = getIntegralTypeNoContext(32, Signed);
		auto i32_st   = st(i32_type);

		// Get all symbol types before entering query context
		const auto p_type         = getSymbolTypeOf("p", body_scope);
		const auto from_ref_type  = getSymbolTypeOf("from_ref", body_scope);
		const auto from_addr_type = getSymbolTypeOf("from_addr", body_scope);
		const auto from_box_type  = getSymbolTypeOf("from_box", body_scope);
		const auto c_type         = getSymbolTypeOf("c", body_scope);
		const auto m_type         = getSymbolTypeOf("m", body_scope);
		const auto pp_type        = getSymbolTypeOf("pp", body_scope);
		const auto inner_type     = getSymbolTypeOf("inner", body_scope);
		const auto deref_val_type = getSymbolTypeOf("deref_val", body_scope);
		const auto sum_type       = getSymbolTypeOf("sum", body_scope);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto ptr_i32     = ctx.query<compiler::tsh::QueryPointerType>({ i32_st });
			const auto ptr_ptr_i32 = ctx.query<compiler::tsh::QueryPointerType>({ st(ptr_i32) });
			const auto manyptr_i32 = ctx.query<compiler::tsh::QueryManyPointerType>({ i32_st });
			const auto cptr_i32    = ctx.query<compiler::tsh::QueryCPointerType>({ i32_st });

			const auto ptr_i32_st     = st(ptr_i32);
			const auto ptr_ptr_i32_st = st(ptr_ptr_i32);
			const auto manyptr_i32_st = st(manyptr_i32);
			const auto cptr_i32_st    = st(cptr_i32);

			ASSERT_EQUAL(ptr_i32_st, p_type);
			ASSERT_EQUAL(ptr_i32_st, from_ref_type);
			ASSERT_EQUAL(ptr_i32_st, from_addr_type);
			ASSERT_EQUAL(ptr_i32_st, from_box_type);
			ASSERT_EQUAL(cptr_i32_st, c_type);
			ASSERT_EQUAL(manyptr_i32_st, m_type);
			ASSERT_EQUAL(ptr_ptr_i32_st, pp_type);
			ASSERT_EQUAL(ptr_i32_st, inner_type);
			ASSERT_EQUAL(i32_st, deref_val_type);
			ASSERT_EQUAL(i32_st, sum_type);
		});

		auto& function = hout.functions.at(0);
		ASSERT_EQUAL(base::StrID("test_pointers"), function->declaration->original_name);

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

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_ref"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			ASSERT_EQUAL(compiler::tsh::Kind::Pointer, cast_ptr->target_type.getType().getKind());
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_addr"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("from_box"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("c"));
			auto cast_ptr = dynamic_cast<const compiler::helios::code::CastExpr*>(expr_ptr.get());
			ASSERT_TRUE(cast_ptr != nullptr);
			ASSERT_EQUAL(compiler::tsh::Kind::CPointer, cast_ptr->target_type.getType().getKind());
		}

		{
			auto expr_ptr  = get_var_init_expr(base::StrID("deref_val"));
			auto deref_ptr = dynamic_cast<const compiler::helios::code::DerefExpr*>(expr_ptr.get());
			ASSERT_TRUE(deref_ptr != nullptr);
			ASSERT_EQUAL(i32_st, deref_ptr->expression_type.getSymbolType().withMutability(Mutable));
		}

		{
			auto expr_ptr = get_var_init_expr(base::StrID("sum"));
			auto bin_ptr
				= dynamic_cast<const compiler::helios::code::BinaryOperatorExpr*>(expr_ptr.get());
			ASSERT_TRUE(bin_ptr != nullptr);
			auto deref_lhs
				= dynamic_cast<const compiler::helios::code::DerefExpr*>(bin_ptr->lhs.get());
			ASSERT_TRUE(deref_lhs != nullptr);
			ASSERT_EQUAL(i32_st, deref_lhs->expression_type.getSymbolType().withMutability(Mutable));
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
				assertEqual(
					actual_type,
					expected_type,
					message + " Expected: " + expected_type.toString()
						+ ", Actual: " + actual_type.toString()
				);
			};

			const auto meta_st     = stConst(compiler::tsh::getMetaType());
			const auto unit_st     = stConst(compiler::tsh::getUnitType());
			const auto int_st      = stConst(compiler::tsh::getIntegralType(ctx, 32, Signed));
			const auto tuple_ii_st = stConst(ctx.query<compiler::tsh::QueryTupleType>(
				{ { int_st.withMutability(Mutable), int_st.withMutability(Mutable) } }
			));
			const auto tuple_tt_st = stConst(ctx.query<compiler::tsh::QueryTupleType>(
				{ { meta_st.withMutability(Mutable), meta_st.withMutability(Mutable) } }
			));

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
				if (var.illegalAccess().value()->getName().illegalAccess().value()->unwrap() == name)
					return var.illegalAccess();
			return {};
		};

		auto get_source_pos_from_origin
			= [](const compiler::helios::code::ElementOrigin& origin) -> dia::SourcePosition {
			return std::any_cast<dia::SourcePosition>(query::utils::withContextCompute(
				[&](query::Context& ctx) { return origin.getSourcePosition(ctx).value(); }
			));
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

			auto expr_pos_from_origin = get_source_pos_from_origin(initial_value_expr->origin);
			assertEqual(
				expr_pos_from_origin,
				initial_value_pst->getSourcePosition().illegalAccess(),
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
				if (fun.illegalAccess().value()->getName().illegalAccess().value()->unwrap() == name)
					return fun.illegalAccess();
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
				get_source_pos_from_origin(fun->origin),
				pst_fun->getSourcePosition().illegalAccess(),
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

	void testAliases() {
		// @TODO: #1412 make this less of a stub once proper dealias lands
		auto [_, root_scope] = getModule(fs::File(path("test_modules/aliases")));

		auto nonwild_using        = getChain("c", root_scope);
		auto nonwild_using_target = getChain("M.c", root_scope);
		ASSERT_EQUAL(nonwild_using, nonwild_using_target);
	}

	void testBackendDependentCompTime() {
		auto [module, root_scope] = getModule(fs::File(path("test_modules/backend_dependent")));

		// `const VALUE = getValue.func();` calls a `@backend_dependent` function. Comp time
		// evaluation runs on the DVM, so it must select the `@dvm_only_impl` implementation
		// (returning 10) rather than the `@native_only_impl` one (returning 20). Evaluating
		// the const therefore checks that comp time picks the right implementation.
		ASSERT_EQUAL(10, getConstValueAs<i32>("VALUE", root_scope));
	}

	void testTemplates() {
		auto [module_id, root_scope] = getModule(fs::File(path("test_modules/templates")));

		// `Number:{1i64}.inner` and `Number:{2i64}.inner` each bake a distinct instantiation of
		// the `Number` template namespace and evaluate the resulting constant.
		ASSERT_EQUAL(1, getConstValueAs<i64>("one", root_scope));
		ASSERT_EQUAL(2, getConstValueAs<i64>("two", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("three_1", root_scope));
		ASSERT_EQUAL(3, getConstValueAs<i64>("three_2", root_scope));

		const auto number_template = getChain("Number", root_scope).back();
		ASSERT_EQUAL(kind(number_template), compiler::helios::SymbolKind::Template);

		query::utils::withContextDo([&](query::Context& ctx) {
			namespace templates = compiler::helios::templates;

			// The declared signature of `template(a: i64)` should carry a single parameter `a`,
			// baked to an immutable `i64` constant.
			auto number_signature
				= templates::getTemplateDeclarationSignature(ctx, number_template).valueOrPanic();

			ASSERT_EQUAL(number_signature.parameters.size(), 1u);
			ASSERT_EQUAL_PRINT(number_signature.parameters.at(0).name, base::StrID("a"));
			ASSERT_EQUAL(
				number_signature.parameters.at(0).type,
				stConst(compiler::tsh::getIntegralType(ctx, 64, Signed))
			);

			// Baking the same template symbol with equal arguments must return
			// the exact same symbol, while different arguments must produce distinct symbols.

			const auto bake = [&](i64 value) {
				const templates::TemplateBakeKey key{
					.template_sym_id = number_template,
					.template_arguments
					= { compiler::ctv::CompileTimeValue(compiler::numeric_value::NumericValue(value)
					) },
				};
				return ctx.query<templates::QueryBakeTemplateSymID>(key).valueOrThrow();
			};

			const auto baked_one       = bake(1);
			const auto baked_one_again = bake(1);
			const auto baked_two       = bake(2);

			ASSERT_EQUAL(baked_one, baked_one_again);
			ASSERT_TRUE(baked_one != baked_two);

			ASSERT_EQUAL(kind(baked_one), compiler::helios::SymbolKind::Namespace);
			ASSERT_EQUAL(kind(baked_two), compiler::helios::SymbolKind::Namespace);
		});

		// Here we test that the HOUT for the module is generated and contains the baked template
		// instantiations. Adjust this as needed, if strategy for template codegen location changes.
		auto hout_module
			= query::entryPoint<compiler::helios::QueryModuleHOUT>(module_id)->valueOrPanic();

		// Just `foo` function.
		ASSERT_EQUAL_PRINT(hout_module.functions.size(), 1);

		// 4 constants + 1 weak const (Number:{1}.inner) added to the module, because it is used by
		// `foo`.
		ASSERT_EQUAL_PRINT(hout_module.glob_data.size(), 4 + 1);
	}

	void testOperatoriness() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/operatoriness")));
		using Operatoriness  = compiler::helios::HOUTFunctionDeclaration::Operatoriness;

		// Plain identifier, 2 params: not an operator.
		ASSERT_EQUAL(
			Operatoriness::None,
			query::entryPoint<compiler::helios::QueryDeclOfFun>(getChain("foo", root_scope).back())
				->valueOrThrow()
				.operatoriness
		);

		// Free operator function, 2 params: infix.
		ASSERT_EQUAL(
			Operatoriness::Infix,
			query::entryPoint<compiler::helios::QueryDeclOfFun>(getChain("+*", root_scope).back())
				->valueOrThrow()
				.operatoriness
		);

		// Free operator function, 1 param: prefix (fixity keywords don't exist yet, so a
		// single-parameter operator name is assumed prefix).
		ASSERT_EQUAL(
			Operatoriness::Prefix,
			query::entryPoint<compiler::helios::QueryDeclOfFun>(getChain("-*", root_scope).back())
				->valueOrThrow()
				.operatoriness
		);

		// Operator methods: 1 explicit param + implicit `self` = infix; 0 explicit params +
		// implicit `self` = prefix.
		auto foo_class = getChain("Foo", root_scope).back();
		auto foo_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(foo_class)->valueOrThrow();
		ASSERT_EQUAL(2, foo_class_info.methods.size());

		auto find_method = [&](std::string_view name) {
			for (const auto& method: foo_class_info.methods)
				if (compiler::helios::name(method) == base::StrID(name)) return method;
			fail(base::strConcat("Method ", name, " not found"));
			return foo_class_info.methods.at(0);
		};

		ASSERT_EQUAL(
			Operatoriness::Infix,
			query::entryPoint<compiler::helios::QueryDeclOfFun>(find_method("+*"))
				->valueOrThrow()
				.operatoriness
		);
		ASSERT_EQUAL(
			Operatoriness::Prefix,
			query::entryPoint<compiler::helios::QueryDeclOfFun>(find_method("-*"))
				->valueOrThrow()
				.operatoriness
		);

		// @TODO: #3131 Add cases for suffix operators.
	}

	void testMethodOperatorResolution() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/method_operators")));
		using namespace compiler::helios::code;

		auto foo_class = getChain("Foo", root_scope).back();
		auto foo_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(foo_class)->valueOrThrow();
		ASSERT_EQUAL(2, foo_class_info.methods.size());

		auto find_method = [&](std::string_view name) {
			for (const auto& method: foo_class_info.methods)
				if (compiler::helios::name(method) == base::StrID(name)) return method;
			fail(base::strConcat("Method ", name, " not found"));
			return foo_class_info.methods.at(0);
		};
		auto infix_method_sym  = find_method("+*");
		auto prefix_method_sym = find_method("-*");

		auto get_return_call = [&](compiler::helios::SymID fn_sym) -> const CallExpr& {
			const auto& fn_hout
				= query::entryPoint<compiler::helios::QueryCodeOfFun>({ fn_sym })->valueOrPanic();
			ASSERT_EQUAL(2, fn_hout.body->statements.size());
			const auto* ret_stmt
				= dynamic_cast<const ReturnStmt*>(fn_hout.body->statements.at(1).get());
			ASSERT_TRUE(ret_stmt != nullptr);
			const auto* call_expr
				= dynamic_cast<const CallExpr*>(stripImplicitMove(ret_stmt->value.get()));
			ASSERT_TRUE(call_expr != nullptr);
			return *call_expr;
		};

		// `foo +* 5` inside useInfix must resolve to Foo's `+*` method, with `foo` self-bound as a
		// reference (exactly like a regular method call `foo.someMethod()` would bind `self`).
		const auto& infix_call   = get_return_call(getChain("useInfix", root_scope).back());
		const auto* infix_callee = dynamic_cast<const IdentifierExpr*>(infix_call.callee.get());
		ASSERT_TRUE(infix_callee != nullptr);
		ASSERT_EQUAL(infix_method_sym, infix_callee->symbol);
		ASSERT_EQUAL(2, infix_call.arguments.size());
		ASSERT_TRUE(dynamic_cast<const RefOfExpr*>(infix_call.arguments.at(0).get()) != nullptr);

		// `-*foo` inside usePrefix must resolve to Foo's `-*` method, with `foo` self-bound the
		// same way.
		const auto& prefix_call   = get_return_call(getChain("usePrefix", root_scope).back());
		const auto* prefix_callee = dynamic_cast<const IdentifierExpr*>(prefix_call.callee.get());
		ASSERT_TRUE(prefix_callee != nullptr);
		ASSERT_EQUAL(prefix_method_sym, prefix_callee->symbol);
		ASSERT_EQUAL(1, prefix_call.arguments.size());
		ASSERT_TRUE(dynamic_cast<const RefOfExpr*>(prefix_call.arguments.at(0).get()) != nullptr);

		// @TODO: #3131 Add case for suffix operator.
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

			auto scope = maybe_scope.value();
			Ref  symbols_in_scope
				= &query::entryPoint<compiler::helios::QuerySymbolsInScope>(scope)->valueOrPanic();

			auto found = false;
			for (auto s: *symbols_in_scope) {
				if (s == symbol) {
					found = true;
					break;
				}
			}
			assertTrue(
				found,
				std::string("Symbol was not found in its scope: ")
					+ compiler::helios::name(symbol).str()
			);
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
