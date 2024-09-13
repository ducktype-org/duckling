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
#include <typesystem/typesystem.hpp>
#include <typesystem/internal/queries.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include "base/variant.hpp"
using namespace compiler::helios::test_utils;

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("HeliosTests") {
		lexer::init();
		pst::init();
		ts::init();

		TESTER_ADD_TEST(errorTests);
		TESTER_ADD_TEST(testI32Consts);
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testStructSymbolData);
		TESTER_ADD_TEST(testTypeOf);
		TESTER_ADD_TEST(simpleHOUTTest);
		TESTER_ADD_TEST(importTest);
		TESTER_ADD_TEST(houtVisitorTest);
		TESTER_ADD_TEST(exprTreeTest);
	}

private:
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

	void testStructSymbolData() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/structs")));

		const auto first_struct = getChain("FirstStructEver", root_scope).back();
		UNPACK_THROW(
			const auto first_struct_info =,
			query::entryPoint<compiler::helios::QueryStructSymbolData>(first_struct)
		);
		UNPACK_THROW(
			const auto first_struct_typeinfo =,
			query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_struct)
		);

		ASSERT_EQUAL(2, first_struct_info.members.size());
		ASSERT_EQUAL(2, first_struct_info.methods.size());
		ASSERT_EQUAL(0, first_struct_info.bases.size());
		ASSERT_EQUAL("FirstStructEver", first_struct_info.name);

		const auto second_struct = getChain("SecondStruct", root_scope).back();

		UNPACK_THROW(
			auto second_struct_info =,
			query::entryPoint<compiler::helios::QueryStructSymbolData>(second_struct)
		);

		ASSERT_EQUAL(0, second_struct_info.members.size());
		ASSERT_EQUAL(0, second_struct_info.methods.size());
		ASSERT_EQUAL(1, second_struct_info.bases.size());
		ASSERT_EQUAL(true, first_struct_typeinfo == second_struct_info.bases.front());
		ASSERT_EQUAL("SecondStruct", second_struct_info.name);
	}

	void testTypeOf() {
		auto [_, root_scope] = getModule(fs::FilePath(path("test_modules/types")));

		const auto INT32_TYPE = query::entryPoint<ts::QueryIntegralType>({ 32, true });
		const auto F32_TYPE   = query::entryPoint<ts::QueryFloatType>(32);

		ASSERT_EQUAL(true, INT32_TYPE == getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(true, F32_TYPE == getTypeOf("SimpleFloat", root_scope));

		const auto tuple_int_int           = getTypeOf("TupleII", root_scope);
		const auto tuple_int_int_type_info = query::entryPoint<ts::QueryTupleType>(
			{ { { INT32_TYPE, false }, { INT32_TYPE, false } } }
		);
		ASSERT_EQUAL(true, tuple_int_int == tuple_int_int_type_info);

		const auto first_variant = getTypeOf("first_variant", root_scope);
		const auto first_variant_type_info
			= query::entryPoint<ts::QueryVariantType>({ { INT32_TYPE, F32_TYPE } });
		ASSERT_EQUAL(true, first_variant == first_variant_type_info);

		const auto weird_variant = getTypeOf("weird_variant", root_scope);

		const auto structA     = getTypeFromDefinition("A", root_scope);
		const auto structB     = getTypeFromDefinition("B", root_scope);
		const auto structC     = getTypeFromDefinition("C", root_scope);
		auto       right_tuple = query::entryPoint<ts::QueryTupleType>(
            { { { structA, false },
		              { query::entryPoint<ts::QueryVariantType>({ { structB, structC } }), false } } }
        );
		const auto weird_variant_type
			= query::entryPoint<ts::QueryVariantType>({ { structA, right_tuple } });
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
			auto name = base::StrId(str);
			for (auto& gb: hout.glob_data) {
				if (gb.original_name == name) {
					this->assert(gb.value == val, "Bad constant value");
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

		auto              sym1 = getChain("V31", root_scope).back();
		std::stringstream out;
		UNPACK_THROW(
			auto tree1 =, query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym1)
		);

		tree1->debugPrint(out);
		ASSERT_EQUAL("(3+((5+9)*2))", out.str());

		ASSERT_EQUAL(12, getValue("V12", root_scope));
		auto sym2 = getChain("V12", root_scope).back();
		UNPACK_THROW(
			auto tree2 =, query::entryPoint<compiler::helios::QueryHOUTExprTreeOfSym>(sym2)
		);
		std::stringstream out2;
		tree2->debugPrint(out2);

		auto sym3       = getChain("N.V3", root_scope).back();
		auto symV3_repr = base::strConcat("(Symbol ", sym3.customPerfectHash(), ")");
		ASSERT_EQUAL(
			base::strConcat("(", symV3_repr, "+(", symV3_repr, "*", symV3_repr, "))"), out2.str()
		);
	}

	void errorTests() {
		using namespace compiler::helios;
		auto [_, root_scope]
			= getModule(fs::FilePath(path("test_modules/error_generating/bad_expr")));

		try {
			getValue("InvalidExpr", root_scope);
		} catch (QueryConstValueOf_Result::error_type& err) {
			variant_match(err) {
				variant_case(errors::ExpressionParsingError, parsing_err)
					ASSERT_EQUAL_NO_PRINT(parsing_err.message, "Malformed expression");

				variant_default RIFT_PANIC("Caught invalid error in tests");
			}
		}

		try {
			getValue("InvalidSym", root_scope);
		} catch (QueryConstValueOf_Result::error_type& err) {
			// We might need something like:
			// https://stackoverflow.com/questions/39272268/creating-a-new-boost-variant-type-from-given-nested-boost-variant-type
			variant_match(err) {
				variant_case(QueryLookup_Result::error_type, lookup_error) {
					variant_match(lookup_error) {
						variant_case(errors::SymbolNotFoundError, symbol_error) {
							// Since this branch was chosen, everything worked well.
						}
						variant_default RIFT_PANIC("Caught invalid error in tests");
					}
				}

				variant_default RIFT_PANIC("Caught invalid error in tests");
			}
		}
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
