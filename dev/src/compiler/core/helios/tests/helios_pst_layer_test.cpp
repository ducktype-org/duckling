
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios_private/pst_layer/for_all.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler;
using namespace compiler::helios::test_utils;

class HeliosErrorsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosErrorsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testPstForAll);
		TESTER_ADD_TEST(testGetStmtsFromStmtAggregate);
	}

private:
	void testPstForAll() {}

	void testGetStmtsFromStmtAggregate() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/pst_layer/get_stmts")));

		auto n_namespace = getChain("N", root_scope).back();

		auto n_pst = compiler::helios::symbolPst(n_namespace)
		                 ->dynamicCast<pst::Namespace>()
		                 .illegalAccess()
		                 .value();

		auto n_body = n_pst->getBody().illegalAccess().value();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto n_body_stmts = compiler::helios::getStmtsFromStmtAggregate(ctx, n_body);

			ASSERT_EQUAL(n_body_stmts.size(), 4);

			std::vector<std::string> expected_stmt_names = { "a", "b", "c", "N" };

			for (size_t i = 0; i < n_body_stmts.size(); i++) {
				auto stmt        = n_body_stmts.at(i).unlock(ctx);
				auto stmt_symbol = ctx.query<compiler::helios::QuerySymbolOfSTMT>(stmt);
				ASSERT_EQUAL(
					compiler::helios::name(stmt_symbol.valueOrPanic()),
					base::StrID(expected_stmt_names.at(i))
				);
			}
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
