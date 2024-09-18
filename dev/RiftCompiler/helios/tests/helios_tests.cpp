#include <helios/scope_symbol_id.hpp>
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
#include <typesystem/higher/typesystem.hpp>
#include <typesystem/higher/internal/queries.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("HeliosTests") {
		lexer::init();
		pst::init();
		tsh::init();

		TESTER_ADD_TEST(testI32Consts);
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testClassSymbolData);
		TESTER_ADD_TEST(testTypeOf);
		TESTER_ADD_TEST(simpleHOUTTest);
		TESTER_ADD_TEST(importTest);
		TESTER_ADD_TEST(houtVisitorTest);
		TESTER_ADD_TEST(exprTreeTest);
		TESTER_ADD_TEST(houtVariablesTest);
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
		ASSERT_EQUAL(std::numeric_limits<i32>::max(), getValue("MAX_I32", root_scope));
		ASSERT_EQUAL(3, getValue("H2", root_scope));
		ASSERT_EQUAL(1, getValue("T0", root_scope));
		ASSERT_EQUAL(2, getValue("T1", root_scope));
		ASSERT_EQUAL(3, getValue("T2", root_scope));
		ASSERT_EQUAL(75, getValue("F", root_scope));
	}

	void testClassSymbolData() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/classes")));

		const auto first_class = getChain("FirstClassEver", root_scope).back();
		const auto first_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(first_class);
		const auto first_class_typeinfo
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_class);

		ASSERT_EQUAL(2, first_class_info.members.size());
		ASSERT_EQUAL(2, first_class_info.methods.size());
		ASSERT_EQUAL(1, first_class_info.constructors.size());
		ASSERT_TRUE(first_class_info.destructor.has_value());
		ASSERT_TRUE(not first_class_info.base.has_value());
		ASSERT_EQUAL(0, first_class_info.implements.size());
		ASSERT_EQUAL("FirstClassEver", first_class_info.name);

		const auto second_class = getChain("SecondClass", root_scope).back();
		auto       second_class_info
			= query::entryPoint<compiler::helios::QueryClassSymbolData>(second_class);

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
	}

	void testEdgeEvals() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/edge_evals")));

		ASSERT_EQUAL(1, getValue("M1", root_scope));
		ASSERT_EQUAL(6, getValue("M2", root_scope));
		ASSERT_EQUAL(7, getValue("O1", root_scope));
		ASSERT_EQUAL(7, getValue("O2", root_scope));
		ASSERT_EQUAL(7, getValue("O3", root_scope));
		ASSERT_EQUAL(7, getValue("O4", root_scope));
	}

	void simpleHOUTTest() {
		auto [module, _] = getModule(fs::FilePath(path("test_modules/hout_simple_test")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 3);
		ASSERT_EQUAL(hout.glob_data.size(), 2);
	}

	void houtVisitorTest() {
		auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/visitor_test_module"))
		);

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 1);

		auto the_function = hout.functions.at(0);

		auto& stmt_list = the_function.body.body->statements;
		ASSERT_EQUAL(stmt_list.size(), 5);

		using namespace compiler::helios::code;

		struct StmtVisitor: public HoutStmtPanickyVisitor {
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

		struct ExprVisitor: public HoutExprPanickyVisitor {
			usize const_int_count = 0;
			usize ident_count     = 0;

			void visitLiteralValueExpr(const LiteralValueExpr&) override { const_int_count++; }

			void visitIdentifierExpr(const IdentifierExpr&) override { ident_count++; }

			void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override { ident_count++; }
		};

		struct ExprVisitorRunner: public HoutStmtPanickyVisitor {
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

	void importTest() {
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

	void exprTreeTest() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/expressions")));
		ASSERT_EQUAL(31, getValue("V31", root_scope));

		auto              sym1  = getChain("V31", root_scope).back();
		auto              tree1 = query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym1);
		std::stringstream out;
		tree1->debugPrint(out);
		ASSERT_EQUAL("(3+((5+9)*2))", out.str());

		ASSERT_EQUAL(12, getValue("V12", root_scope));
		auto              sym2  = getChain("V12", root_scope).back();
		auto              tree2 = query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym2);
		std::stringstream out2;
		tree2->debugPrint(out2);

		auto sym3       = getChain("N.V3", root_scope).back();
		auto symV3_repr = base::strConcat("(Symbol ", sym3.customPerfectHash(), ")");
		ASSERT_EQUAL(
			base::strConcat("(", symV3_repr, "+(", symV3_repr, "*", symV3_repr, "))"), out2.str()
		);
	}

	void houtVariablesTest() {
		auto [module, _] = getModule(fs::FilePath(path("test_modules/variables")));

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 1);

		// @TODO: there is no sane way of accessing variables from HOUT functions
		// to make some assertions on them.
		// For now this test mostly checks that HELIOS does not hit
		// any of its own panics or asserts (which happened a lot during coding of first hout variables).
		// In the future some interface should be added that would allow for better testing of this.
		// For example:
		// * internal way of looking up inside functions (slight more complicated but potentially better).

		auto& function = hout.functions.at(0);
		
		ASSERT_EQUAL(function.original_name, "foo");

		// note that alias should not be included here:
		ASSERT_EQUAL(function.body.body->statements.size(), 6);

		auto& statements = function.body.body->statements;

		auto get_var_ref = [&](usize i) -> decltype(auto) {
			return dynamic_cast<const compiler::helios::code::VariableStmt&>(*statements.at(i).get());
		};

		{
			auto& stmt_0 = get_var_ref(0);
			ASSERT_EQUAL(compiler::helios::name(stmt_0.helios_symbol), "a");

		}


	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
