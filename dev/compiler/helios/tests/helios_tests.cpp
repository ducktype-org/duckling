#include <base/optional.hpp>
#include <helios/helios_errors.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>
#include <helios/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <type_traits>
#include <typesystem/higher/all.hpp>
#include <typesystem/higher/internal/queries.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include <base/variant.hpp>
#include <base/box.hpp>
#include <helios/helios_result.hpp>

using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		lexer::init();
		pst::init();
		tsh::init();

		TESTER_ADD_TEST(testImport);
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testError);
		TESTER_ADD_TEST(testI32Consts);
		TESTER_ADD_TEST(testClassSymbolData);
		TESTER_ADD_TEST(testHoutVariables);
		TESTER_ADD_TEST(testExprTree);
		TESTER_ADD_TEST(TestSimpleHOUT);
		TESTER_ADD_TEST(TestHoutVisitor);
		TESTER_ADD_TEST(TestHeliosResultConcept);
		TESTER_ADD_TEST(TestHeliosResult);
		TESTER_ADD_TEST(testTypeOf);

		// this is at the end
		// so we test all the scopes created in helios tests:
		TESTER_ADD_TEST(scopeParentsAndDepthTests);
	}

private:
	// @TODO: test_modules/aliases are not used in tests

	void testI32Consts() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/constants")));

		ASSERT_EQUAL(1'107, getValue("M", root_scope));
		ASSERT_EQUAL(1, getValue("N.X", root_scope));
		ASSERT_EQUAL(1, getValue("A", root_scope));
		ASSERT_EQUAL(-3, getValue("B", root_scope));
		ASSERT_EQUAL(-1, getValue("D", root_scope));
		ASSERT_EQUAL(6, getValue("E", root_scope));
		// @TODO: Test below disabled - parser currently does not support power operator.
		// ASSERT_EQUAL(std::numeric_limits<i32>::max(), getValue("MAX_I32", root_scope));
		ASSERT_EQUAL(3, getValue("H2", root_scope));
		ASSERT_EQUAL(1, getValue("T0", root_scope));
		ASSERT_EQUAL(2, getValue("T1", root_scope));
		ASSERT_EQUAL(3, getValue("T2", root_scope));
		ASSERT_EQUAL(30, getValue("F", root_scope));
	}

	void testClassSymbolData() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/classes")));

		const auto first_class = getChain("FirstClassEver", root_scope).back();
		const auto first_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(first_class)
		          ->valueOrThrow();
		const auto first_class_typeinfo
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_class)
		          ->valueOrThrow();

		ASSERT_EQUAL(2, first_class_info.members.size());
		ASSERT_EQUAL(2, first_class_info.methods.size());
		ASSERT_EQUAL(1, first_class_info.constructors.size());
		ASSERT_TRUE(first_class_info.destructor.has_value());
		ASSERT_TRUE(not first_class_info.base.has_value());
		ASSERT_EQUAL(0, first_class_info.implements.size());
		ASSERT_EQUAL("FirstClassEver", first_class_info.name);

		const auto second_class = getChain("SecondClass", root_scope).back();
		auto       second_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(second_class)
		          ->valueOrThrow();

		ASSERT_EQUAL(0, second_class_info.members.size());
		ASSERT_EQUAL(0, second_class_info.methods.size());
		ASSERT_EQUAL(0, second_class_info.constructors.size());
		ASSERT_TRUE(not second_class_info.destructor.has_value());
		ASSERT_TRUE(second_class_info.base.has_value());
		ASSERT_EQUAL(first_class_typeinfo, second_class_info.base);
		ASSERT_EQUAL("SecondClass", second_class_info.name);
	}

	void testTypeOf() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/types")));

		const auto INT32_TYPE = query::entryPoint<tsh::QueryIntegralType>({ 32, true });
		const auto F32_TYPE   = query::entryPoint<tsh::QueryFloatType>(32);
		const auto BOOL_TYPE  = query::entryPoint<tsh::QueryBoolType>({});
		const auto META_TYPE  = query::entryPoint<tsh::QueryMetaType>({});

		ASSERT_EQUAL(true, INT32_TYPE == getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(true, F32_TYPE == getTypeOf("SimpleFloat", root_scope));

		const auto tuple_int_int           = getTypeOf("TupleII", root_scope);
		const auto tuple_int_int_type_info = query::entryPoint<tsh::QueryTupleType>(
			{ { { INT32_TYPE, false }, { INT32_TYPE, false } } }
		);
		ASSERT_EQUAL(true, tuple_int_int == tuple_int_int_type_info);

		const auto first_variant = getTypeOf("first_variant", root_scope);
		const auto first_variant_type_info
			= query::entryPoint<tsh::QueryVariantType>({ { INT32_TYPE, F32_TYPE } });
		ASSERT_EQUAL(true, first_variant == first_variant_type_info);

		const auto second_variant = getTypeOf("second_variant", root_scope);
		const auto second_variant_type_info
			= query::entryPoint<tsh::QueryVariantType>({ { INT32_TYPE, F32_TYPE, BOOL_TYPE } });
		ASSERT_EQUAL(true, second_variant == second_variant_type_info);

		const auto weird_variant = getTypeOf("weird_variant", root_scope);

		const auto classA      = getTypeFromDefinition("A", root_scope);
		const auto classB      = getTypeFromDefinition("B", root_scope);
		const auto classC      = getTypeFromDefinition("C", root_scope);
		auto       right_tuple = query::entryPoint<tsh::QueryTupleType>(
            { { { classA, false },
		              { query::entryPoint<tsh::QueryVariantType>({ { classB, classC } }), false } } }
        );
		const auto weird_variant_type
			= query::entryPoint<tsh::QueryVariantType>({ { classA, right_tuple } });
		ASSERT_EQUAL(true, weird_variant == weird_variant_type);

		ASSERT_EQUAL(META_TYPE, getTypeOf("T", root_scope));
	}

	void testEdgeEvals() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/edge_evals")));
		ASSERT_EQUAL(1, getValue("M1", root_scope));
		ASSERT_EQUAL(6, getValue("M2", root_scope));
		ASSERT_EQUAL(7, getValue("O1", root_scope));
		ASSERT_EQUAL(7, getValue("O2", root_scope));
		// These do not work anymore.
		// ASSERT_EQUAL(7, getValue("O3", root_scope));
		// ASSERT_EQUAL(7, getValue("O4", root_scope));
	}

	void TestSimpleHOUT() {
		auto [module, _] = getModule(fs::FilePath(path("test_modules/hout_simple_test")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 3);
		ASSERT_EQUAL(hout.glob_data.size(), 2);

		// just for cov and to see if it does not throw:
		[[maybe_unused]] auto hout_debug_print = hout.debugPrint();
	}

	void TestHoutVisitor() {
		auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/visitor_test_module"))
		);

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 1);

		auto the_function = hout.functions.at(0);

		auto& stmt_list = the_function.body.body->statements;
		ASSERT_EQUAL(stmt_list.size(), 5);

		using namespace compiler::helios::code;

		struct StmtVisitor: public HoutStmtVisitorPanicky {
			usize expr_stmt_count        = 0;
			usize return_stmt_count      = 0;
			usize void_return_stmt_count = 0;
			usize if_stmt_count          = 0;

			void visitExprStmt(const ExprStmt&) override { expr_stmt_count++; }

			void visitReturnStmt(const ReturnStmt&) override { return_stmt_count++; }

			void visitVoidReturnStmt(const VoidReturnStmt&) override { void_return_stmt_count++; }

			void visitIfStmt(const IfStmt&) override { if_stmt_count++; }
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

		struct ExprVisitor: public HoutExprVisitorPanicky {
			usize const_int_count = 0;
			usize ident_count     = 0;

			void visitLiteralValueExpr(const LiteralValueExpr&) override { const_int_count++; }

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
		auto [module, _] = getModule(fs::FilePath(path("test_modules/import_tests")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		auto test_value = [&](auto str, i64 val) {
			auto name = base::StrID(str);
			for (auto& gb: hout.glob_data) {
				if (gb.original_name == name) {
					this->assertTrue(gb.value == val, "Bad constant value");
					return;
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
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/expressions")));

		ASSERT_EQUAL(1, getValue("V1", root_scope));
		ASSERT_EQUAL(31, getValue("V31", root_scope));

		auto              sym1 = getChain("V31", root_scope).back();
		std::stringstream out;
		auto&             tree1
			= query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym1)->valueOrThrow();

		tree1->debugPrint(out);
		ASSERT_EQUAL("3+((5+9)*2)", out.str());

		ASSERT_EQUAL(12, getValue("V12", root_scope));
		auto  sym2 = getChain("V12", root_scope).back();
		auto& tree2
			= query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym2)->valueOrThrow();
		std::stringstream out2;
		tree2->debugPrint(out2);

		auto sym3       = getChain("N.V3", root_scope).back();
		auto symV3_repr = base::strConcat("(Symbol V3 (", sym3.customPerfectHash(), "))");
		ASSERT_EQUAL(base::strConcat(symV3_repr, "+", symV3_repr, "*", symV3_repr), out2.str());
	}

	void testError() {
		using namespace compiler::helios;

		auto [_, root_scope]
			= getModule(fs::FilePath(path("test_modules/error_generating/bad_expr")));

		try {
			getValue("InvalidExpr", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			getValue("InvalidSym", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}

		try {
			getValue("C", root_scope);
			CORE_PANIC("Should throw.");
		} catch (errors::Failed& err) {
			// Since this branch was chosen, everything worked well.
		}
	}

	void TestHeliosResultConcept() {
		using namespace compiler::helios::errors::impl;

		static_assert(std::is_same_v<
					  std::variant<int, float, bool>,
					  FlattenVariant_t<std::variant<int, float, std::variant<bool>>>>);

		static_assert(IsIn_v<int, int>);
		static_assert(IsIn_v<int, float, double, int>);
		static_assert(IsIn_v<int, float, int, double, int>);
		static_assert(!IsIn_v<int, float, double>);

		static_assert(std::is_same_v<UniqueTypes<int, int>::types, UniqueTypes<int>::types>);
		static_assert(!std::is_same_v<UniqueTypes<int, int>::types, UniqueTypes<float>::types>);
		static_assert(!std::
		                  is_same_v<UniqueTypes<int, int, float>::types, UniqueTypes<int>::types>);

		static_assert(std::is_same_v<UniqueTypesVariant_t<int>, std::variant<int>>);
		static_assert(std::is_same_v<UniqueTypesVariant_t<int, int>, std::variant<int>>);
		static_assert(!std::is_same_v<UniqueTypesVariant_t<int, int, float>, std::variant<int>>);
		static_assert(std::is_same_v<
					  UniqueTypesVariant_t<int, int, float>,
					  std::variant<int, float>>);
		static_assert(std::is_same_v<
					  UniqueTypesVariant_t<int, int, float, int, int>,
					  std::variant<float, int>>);
		static_assert(std::is_same_v<
					  UniqueTypesVariant_t<int, int, float, std::variant<int, int>>,
					  std::variant<float, int>>);
		static_assert(std::is_same_v<
					  UniqueTypesVariant_t<
						  std::variant<int, float, int>,
						  int,
						  int,
						  float,
						  std::variant<int, int>>,
					  std::variant<float, int>>);

		struct A {};

		std::variant<std::variant<int, float>, std::variant<int, A>> y;

		UniqueTypesVariant_t<decltype(y)> y1 = 1;

		variant_match(y1) {
			variant_case(int, val) ASSERT_EQUAL(val, 1);
			variant_default CORE_PANIC("Invalid state");
		}

		static_assert(std::is_same_v<
					  std::variant<int, float, bool>,
					  UniqueTypesVariant_t<
						  std::variant<std::variant<int, float, std::variant<bool>>>>>);
	}

	void TestHeliosResult() {
		using namespace compiler::helios::errors;

		static_assert(std::is_same_v<HResult<int, int>::ErrorType, int>);
		static_assert(std::is_same_v<HResult<int, std::variant<int>>::ErrorType, int>);
		static_assert(std::is_same_v<
					  HResult<int, int, std::variant<float>>::ErrorType,
					  std::variant<int, float>>);
		static_assert(std::is_same_v<HResult<int, int, bool>::ErrorType, std::variant<int, bool>>);
		static_assert(std::is_same_v<
					  HResult<int, int, int, int, float>::ErrorType,
					  std::variant<int, float>>);
		// static_assert(std::is_same_v<impl::flatten::FlattenVariant_t<int, int>,
		// impl::FlattenVariant_t<typename T>)

		HResult<int, float> hr1 = 1;
		ASSERT_TRUE(hr1.hasValue());
		ASSERT_TRUE(bool(hr1));
		ASSERT_TRUE(!hr1.hasError());
		ASSERT_EQUAL(1, hr1.value());

		base::Optional<base::Ref<int>> opt1 = hr1.optValue();
		ASSERT_TRUE(opt1.has_value());
		ASSERT_EQUAL(1, **opt1);

		HResult<std::string, float> hr2        = "Value";
		base::Optional<std::string> stolen_opt = std::move(hr2).optValueMove();
		ASSERT_EQUAL("Value", stolen_opt);

		std::string                    info  = "Hello";
		HResult<int, std::string_view> whoa2 = HError(std::string_view(info));
		ASSERT_TRUE(!whoa2.hasValue());
		ASSERT_TRUE(whoa2.hasError());
		ASSERT_TRUE(!bool(whoa2));
		ASSERT_EQUAL(whoa2.error(), "Hello");

		struct Err1 {};

		struct Err2 {};

		struct Err3 {};

		struct Err4 {};

		HResult<int, Err2, Err4> sub_result = HError(Err2());
		static_assert(std::is_same_v<decltype(sub_result)::ErrorType, std::variant<Err2, Err4>>);
		HResult<int, Err1, Err2, Err3, decltype(sub_result)::ErrorType> result(sub_result);
		static_assert(std::is_same_v<
					  decltype(result)::ErrorType,
					  std::variant<Err1, Err3, Err2, Err4>>);
		bool entered2 = false;
		ASSERT_TRUE(!result.hasValue());
		ASSERT_TRUE(result.hasError());
		variant_match(result.error()) {
			variant_case(Err2, value) { entered2 = true; }
			variant_default CORE_PANIC("Invalid branch");
		}
		ASSERT_TRUE(entered2);
	}

	void testHoutVariables() {
		auto [module, _] = getModule(fs::FilePath(path("test_modules/variables")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 1);

		auto& function = hout.functions.at(0);

		ASSERT_EQUAL(function.original_name, "foo");

		// note that alias should not be included here:
		ASSERT_EQUAL(function.body.body->statements.size(), 7);

		auto& statements = function.body.body->statements;

		auto get_var_ref = [&](usize i) -> decltype(auto) {
			return dynamic_cast<const compiler::helios::code::VariableStmt&>(*statements.at(i));
		};


		auto i32_type   = query::entryPoint<tsh::QueryIntegralType>(32);
		auto f32_type   = query::entryPoint<tsh::QueryFloatType>(32);
		auto i32_or_f32 = query::entryPoint<tsh::QueryVariantType>({ { i32_type, f32_type } });

		{
			auto& var = get_var_ref(0);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "a");
			ASSERT_EQUAL(var.type.getType(), i32_type);
		}

		{
			auto& var = get_var_ref(1);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "b");
			ASSERT_EQUAL(var.type.getType(), i32_type);
		}

		{
			auto& var = get_var_ref(2);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "c");
			ASSERT_EQUAL(var.type.getType(), i32_or_f32);
		}

		{
			auto& var = get_var_ref(3);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "d");
			ASSERT_EQUAL(var.type.getType().getKind(), tsh::Kind::Class);
		}

		{
			auto& if_stmt = dynamic_cast<const compiler::helios::code::IfStmt&>(*statements.at(4));
			auto& var     = dynamic_cast<const compiler::helios::code::VariableStmt&>(
                *if_stmt.body.statements.at(0)
            );
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "x");
			ASSERT_EQUAL(var.type.getType(), i32_type);
		}

		{
			auto& var = get_var_ref(5);
			ASSERT_EQUAL(compiler::helios::name(var.helios_symbol), "e");
			ASSERT_EQUAL(var.type.getType().getKind(), tsh::Kind::Class);
		}

		// debug print test just for cov and to see if it does not throw:
		[[maybe_unused]] auto debug_print_out = hout.debugPrint();
	}

	void scopeParentsAndDepthTests() {
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
				// @TODO: make it not print to cerr, but to ostream or string:
				scope.debugPrintScopeAndParents();
			}
			assertTrue(parent(scope).empty(), "Scope at depth 0 can't have a parent");
		}
	}
};

TESTER_COMMON_MAIN("/compiler/helios/tests/");
