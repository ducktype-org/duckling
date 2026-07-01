#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/mangler/mangler.hpp>
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
		TESTER_ADD_TEST(testCounterControlsExpressionWrapperMangling);
		TESTER_ADD_TEST(testSameCounterDifferentTypeCausesManglingCollision);
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

			auto wrapper_result = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 7,
			});
			ASSERT_HAS_VALUE(wrapper_result);
			auto& wrapper = wrapper_result.valueOrPanic();

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

			auto wrapper_result = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 13,
			});
			ASSERT_HAS_VALUE(wrapper_result);
			auto& wrapper = wrapper_result.valueOrPanic();

			auto sym_ref = helios::getSymRef(wrapper.declaration->original_symbol);

			auto repl_data
				= std::get_if<helios::defgen::ReplExpressionWrapper>(&sym_ref->other);
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

			auto wrapper_result_a = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 101,
			});
			auto wrapper_result_b = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_stmt,
				.counter   = 102,
			});
			ASSERT_HAS_VALUE(wrapper_result_a);
			ASSERT_HAS_VALUE(wrapper_result_b);
			auto& wrapper_a = wrapper_result_a.valueOrPanic();
			auto& wrapper_b = wrapper_result_b.valueOrPanic();

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

	void testSameCounterDifferentTypeCausesManglingCollision() {
		query::utils::withContextDo([&](query::Context& ctx) {
			auto expr_i32 = extractSingleExpression(ctx, "42;");
			auto expr_f64 = extractSingleExpression(ctx, "3.14;");

			auto wrapper_result_i32 = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_i32,
				.counter   = 999,
			});
			auto wrapper_result_f64 = ctx.query<repl::QueryReplExpressionWrapper>({
				.expr_stmt = expr_f64,
				.counter   = 999,
			});
			ASSERT_HAS_VALUE(wrapper_result_i32);
			ASSERT_HAS_VALUE(wrapper_result_f64);
			auto& wrapper_i32 = wrapper_result_i32.valueOrPanic();
			auto& wrapper_f64 = wrapper_result_f64.valueOrPanic();

			auto sym_i32 = helios::getSymRef(wrapper_i32.declaration->original_symbol);
			auto sym_f64 = helios::getSymRef(wrapper_f64.declaration->original_symbol);

			auto repl_i32
				= std::get_if<helios::defgen::ReplExpressionWrapper>(&sym_i32->other);
			auto repl_f64
				= std::get_if<helios::defgen::ReplExpressionWrapper>(&sym_f64->other);

			assertTrue(repl_i32 != nullptr && repl_f64 != nullptr, "Expected ReplExpressionWrapper");

			// They have different hashes (good)
			assertTrue(
				gen_i32->queryUnstablePerfectHash() != gen_f64->queryUnstablePerfectHash(),
				"Different return types should produce different hashes"
			);
			// BUT: Their mangled names collide
			auto mangled_i32 = helios::mangler::getSimpleMangledName(
								   ctx, wrapper_i32.declaration->original_symbol
			)
			                       .strView();
			auto mangled_f64 = helios::mangler::getSimpleMangledName(
								   ctx, wrapper_f64.declaration->original_symbol
			)
			                       .strView();

			assertEqual(
				std::string(mangled_i32),
				std::string(mangled_f64),
				"COLLISION: Same counter produces same mangled name despite different return types"
			);
		});
	}

public:
	~HeliosReplExpressionWrapperTests() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/");
