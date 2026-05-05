/**
 * @file helios_stmt_compilation_test.cpp
 * @brief Unit tests for compileSingleStatement (hout_stmt_compilation.cpp).
 *
 * compileSingleStatement is a new entry point (used by the REPL) that is never
 * called through QueryModuleHOUT.
 */

#include <../src_private/helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/query_result.hpp>
#include <tester/tester.hpp>

using namespace compiler;
using namespace compiler::helios;

class HoutStmtCompilationTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HoutStmtCompilationTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testControlFlowStatements);
		TESTER_ADD_TEST(testVariableDeclarations);
		TESTER_ADD_TEST(testReturnStatements);
		TESTER_ADD_TEST(testAssignmentAndExpressions);
		TESTER_ADD_TEST(testBlockExpr);
	}

private:
	/**
	 * @brief Counts each HOUT statement kind seen via acceptVisitor.
	 */
	struct StmtKindCounter final: public code::HoutStmtVisitorPanicky {
		usize variable_count    = 0;
		usize assignment_count  = 0;
		usize if_count          = 0;
		usize while_count       = 0;
		usize return_count      = 0;
		usize void_return_count = 0;
		usize expr_stmt_count   = 0;
		usize block_stmt_count  = 0;

		void visitVariableStmt(const code::VariableStmt&) override { variable_count++; }

		void visitAssignmentStmt(const code::AssignmentStmt&) override { assignment_count++; }

		void visitIfStmt(const code::IfStmt&) override { if_count++; }

		void visitWhileStmt(const code::WhileStmt&) override { while_count++; }

		void visitBlockStmt(const code::BlockStmt&) override { block_stmt_count++; }

		void visitReturnStmt(const code::ReturnStmt&) override { return_count++; }

		void visitVoidReturnStmt(const code::VoidReturnStmt&) override { void_return_count++; }

		void visitExprStmt(const code::ExprStmt&) override { expr_stmt_count++; }
	};

	/**
	 * @brief Navigate to a selected statement inside the first function of a module,
	 * then compile it via compileSingleStatement and return the resulting CodeBlock.
	 *
	 * The caller must be inside a query::utils::withContextDo lambda.
	 */
	static code::CodeBlock compileSingleStatementOfFirstFun(
		query::Context&                   ctx,
		frontend::ModuleID                module_id,
		usize                             stmt_index         = 0,
		base::Optional<tsh::SymbolType<>> custom_return_type = {}
	) {
		// Locate the function symbol
		auto root_scope = queryRootScopeOfMainModuleFile(ctx, module_id);
		auto symbols    = ctx.query<QuerySymbolsInScope>(root_scope)->valueOrPanic();

		base::Optional<SymID> fun_sym_opt;
		for (auto sym: symbols) {
			if (kind(sym) == SymbolKind::Function) {
				fun_sym_opt = sym;
				break;
			}
		}
		CORE_ASSERT(fun_sym_opt.has_value(), "Module must contain at least one function");
		const auto fun_sym = fun_sym_opt.value();

		// Navigate to its PST body
		auto fun_pst = getSymRef(fun_sym)->stmtCast(ctx).value().dynamicCast<pst::Fun>().value();

		auto body_stmts = getStmtsFromStmtAggregate(ctx, fun_pst->getBody());
		CORE_ASSERT(!body_stmts.empty(), "Function body must have at least one statement");

		// Compile the first body statement in isolation
		auto return_type = custom_return_type.has_value() ? custom_return_type.value()
		                                                  : tsh::SymbolType<>{
																tsh::getUnitType(),
																tsh::ReferenceKind::Direct,
																tsh::Mutability::Mutable,
															};
		body_stmts.at(stmt_index).illegalAccess().value()->debugPrint(std::cerr); //value().elementType()();
		return compileSingleStatement(ctx, body_stmts.at(stmt_index), return_type);
	}

	/**
	 * @brief compileSingleStatement correctly handles control flow statements.
	 * Tests: if-statement, while-statement, and if-statement with else clause.
	 */
	void testControlFlowStatements() {
		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					if (1 == 1) {}
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.if_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					while (1 == 1) {}
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.while_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					if (1 == 1) {
						var a = 1;
					} else {
						var b = 2;
					}
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.if_count, 1u);
			});
		}
	}

	/**
	 * @brief compileSingleStatement correctly handles variable declarations.
	 * Tests: variable with initialization, variable without initialization, and const declarations.
	 */
	void testVariableDeclarations() {
		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					var a = 1;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.variable_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					var a: i32;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.variable_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					const a = 1;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 0u);
			});
		}
	}

	/**
	 * @brief compileSingleStatement correctly handles return statements.
	 * Tests: return with value and plain return (void return).
	 */
	void testReturnStatements() {
		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					return 1;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto int_type = tsh::SymbolType<>{
					tsh::getIntegralType(ctx, 32, tsh::IntegralAbstractType::Signedness::Signed),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
				auto block = compileSingleStatementOfFirstFun(ctx, module_id, 0, int_type);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.return_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					return;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.void_return_count, 1u);
			});
		}
	}

	/**
	 * @brief compileSingleStatement correctly handles assignment and expression statements.
	 * Tests: basic assignment, expression statements, and list operations (push/pop).
	 */
	void testAssignmentAndExpressions() {
		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					var a = 1;
					a = 2;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id, 1);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.assignment_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					1 + 1;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.expr_stmt_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					var a: List[i32];
					a += 5;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id, 1);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.expr_stmt_count, 1u);
			});
		}

		{
			auto module_id = frontend::createModuleTreeFromContents(
				R"(
				fun foo() = {
					var a: List[i32];
					a -= 5u64;
				}
			)",
				"test_pkg"
			);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto block = compileSingleStatementOfFirstFun(ctx, module_id, 1);
				ASSERT_EQUAL(block.statements.size(), 1u);
				StmtKindCounter c;
				block.statements.at(0)->acceptVisitor(c);
				ASSERT_EQUAL(c.expr_stmt_count, 1u);
			});
		}
	}

	void testBlockExpr() {
		auto module_id = frontend::createModuleTreeFromContents(
			R"(
				fun foo() = {
					{
						var a = 1;
					};
					{
						var a = 1;
					};
				}
			)",
			"test_pkg"
		);
		query::utils::withContextDo([&](query::Context& ctx) {
			auto block = compileSingleStatementOfFirstFun(ctx, module_id);
			ASSERT_EQUAL(block.statements.size(),1);
			StmtKindCounter c;
			block.statements.at(0)->acceptVisitor(c);
			ASSERT_EQUAL(c.block_stmt_count, 1);


			auto block2 = compileSingleStatementOfFirstFun(ctx, module_id, 1);
			ASSERT_EQUAL(block2.statements.size(),1);
			StmtKindCounter c2;
			block2.statements.at(0)->acceptVisitor(c2);
			ASSERT_EQUAL(c2.block_stmt_count, 1);
		});

		module_id = frontend::createModuleTreeFromContents(
			R"(
				fun foo() = {
					block {
						var a = 1;
					}
					block second {
						var a = 1;
					}
				}
			)",
			"test_pkg"
		);
		query::utils::withContextDo([&](query::Context& ctx) {
			auto block = compileSingleStatementOfFirstFun(ctx, module_id);
			ASSERT_EQUAL(block.statements.size(),1);
			StmtKindCounter c;
			block.statements.at(0)->acceptVisitor(c);
			ASSERT_EQUAL(c.block_stmt_count, 1);


			auto block2 = compileSingleStatementOfFirstFun(ctx, module_id, 1);
			ASSERT_EQUAL(block2.statements.size(),1);
			StmtKindCounter c2;
			block2.statements.at(0)->acceptVisitor(c2);
			ASSERT_EQUAL(c2.block_stmt_count, 1);
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
