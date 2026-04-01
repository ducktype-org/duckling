#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
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
		TESTER_ADD_TEST(testCounterControlsExpressionWrapperMangling);
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

	void testValueExpressionWrapsIntoReturnStmt() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto expr_stmt = extractSingleExpression(ctx, "1 + 2;");

			auto wrapper = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 7,
			});

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

			auto wrapper = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 13,
			});

			auto sym_ref  = helios::getSymRef(wrapper.declaration->original_symbol);
			auto gen_data = std::get_if<helios::defgen::GeneratedSymbolData>(&sym_ref->other);
			assertTrue(gen_data != nullptr, "Expected generated symbol data");

			auto repl_data
				= std::get_if<helios::defgen::GeneratedSymbolData::ReplExpressionWrapper>(
					&gen_data->data
				);
			assertTrue(repl_data != nullptr, "Expected ReplExpressionWrapper generated symbol");
			ASSERT_EQUAL(repl_data->counter, 13u);
			ASSERT_EQUAL(repl_data->return_type, wrapper.declaration->return_type);
			ASSERT_EQUAL(
				helios::kind(wrapper.declaration->original_symbol), helios::SymbolKind::Function
			);
		});
	}

	void testCounterControlsExpressionWrapperMangling() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto expr_stmt = extractSingleExpression(ctx, "1 + 2;");

			auto wrapper_a = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 101,
			});
			auto wrapper_b = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 102,
			});

			auto mangled_a
				= helios::mangler::getSimpleMangledName(ctx, wrapper_a.declaration->original_symbol)
			          .strView();
			auto mangled_b
				= helios::mangler::getSimpleMangledName(ctx, wrapper_b.declaration->original_symbol)
			          .strView();

			assertTrue(mangled_a != mangled_b, "Different counters should produce different names");
			assertTrue(
				std::string(mangled_a).find("__repl_expr_wrapper_101") != std::string::npos,
				"First mangled name should include its counter"
			);
			assertTrue(
				std::string(mangled_b).find("__repl_expr_wrapper_102") != std::string::npos,
				"Second mangled name should include its counter"
			);
		});
	}

public:
	~HeliosReplExpressionWrapperTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/");
