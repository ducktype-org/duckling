#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class HeliosReplExpressionWrapperTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosReplExpressionWrapperTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testValueExpressionWrapsIntoReturnStmt);
		TESTER_ADD_TEST(testWrapperMetadataMatchesGeneratedSymbolData);
	}

private:
	struct StmtKindCounter final: public helios::code::HoutStmtVisitorPanicky {
		usize return_count    = 0;
		usize expr_stmt_count = 0;

		void visitReturnStmt(const helios::code::ReturnStmt&) override { return_count++; }

		void visitExprStmt(const helios::code::ExprStmt&) override { expr_stmt_count++; }
	};

	static pst::AccessLocked<pst::ExprStmt> extractSingleExpression(
		query::Context& ctx, std::string_view code
	) {
		auto module_id = frontend::createModuleTreeFromContents(code, "test_pkg");
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = frontend::getFilePST(ctx, main_file);
		auto expr_opt  = pst::extractSingleExpression(ctx, pst->getRootElement());
		CORE_ASSERT(expr_opt.has_value(), "Expected a single expression statement in test input");
		return expr_opt.value();
	}

	static CRef<query::QResult<helios::HOUTFunction>> expressionWrapper(
		query::Context& ctx, pst::AccessLocked<pst::ExprStmt> expr_stmt
	) {
		auto symbol = repl::queryReplExpressionWrapperSymbol(ctx, expr_stmt);
		CORE_ASSERT(!symbol.hasFailed(), "Expected the expression wrapper symbol to be created");
		return ctx.query<helios::QueryCodeOfFun>(symbol.valueOrPanic());
	}

	void testValueExpressionWrapsIntoReturnStmt() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto expr_stmt = extractSingleExpression(ctx, "1 + 2;");

			auto wrapper_result = expressionWrapper(ctx, expr_stmt);
			ASSERT_HAS_VALUE(*wrapper_result);
			auto& wrapper = wrapper_result->valueOrPanic();

			ASSERT_EQUAL(wrapper.declaration->parameters.size(), 0u);
			ASSERT_EQUAL(wrapper.body->statements.size(), 1u);

			StmtKindCounter counter;
			wrapper.body->statements.at(0)->acceptVisitor(counter);

			ASSERT_EQUAL(counter.return_count, 1u);
			ASSERT_EQUAL(counter.expr_stmt_count, 0u);
		});
	}

	void testWrapperMetadataMatchesGeneratedSymbolData() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto expr_stmt = extractSingleExpression(ctx, "40 + 2;");

			auto wrapper_result = expressionWrapper(ctx, expr_stmt);
			ASSERT_HAS_VALUE(*wrapper_result);
			auto& wrapper = wrapper_result->valueOrPanic();

			auto sym_ref = helios::getSymRef(wrapper.declaration->original_symbol);

			auto repl_data = std::get_if<helios::defgen::ReplInputWrapper>(&sym_ref->other);
			assertTrue(repl_data != nullptr, "Expected ReplInputWrapper generated symbol");
			assertTrue(
				std::holds_alternative<helios::defgen::ReplInputWrapper::Expression>(
					repl_data->element
				),
				"Expected an expression wrapper"
			);
			ASSERT_EQUAL(
				helios::kind(wrapper.declaration->original_symbol), helios::SymbolKind::Function
			);
		});
	}

public:
	~HeliosReplExpressionWrapperTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/");
