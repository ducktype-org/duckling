#include "helios/scope_symbol_id.hpp"
#include "helios/scopes/scopes.hpp"
#include "helios/symbols/symbols.hpp"
#include "query_framework/query_entry_point.hpp"
#include <tester/tester.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>

class HeliosTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("HeliosTests") {
		lexer::init();
		pst::init();
		ts::init();

		TESTER_ADD_TEST(testQuerySymbolOfSTMT);
	}

private:
	std::pair<pst::PST, const fs::FilePath&> prepare(const fs::FilePath& file) {
		auto td     = lexer::tokenizeFile(file);
		auto parsed = pst::parse(std::move(td));
		assert(parsed.getLogger().good(), "there are unexpected errors in rift source-code");
		return { std::move(parsed), file };
	}

	void testQuerySymbolOfSTMT() {
		auto [pst, file]  = prepare(fs::FilePath(path("constants/constants.rmf")));
		auto   root_scope = pst.getTopLevelElement();
		auto&& stmts      = root_scope->getStatements();
		// @TODO: Write a QuerySymbolOfSTMT test here.
		// EDIT: I don't know how to construct ScopeID for QuerySymbolOfSTMT key...
		// for (const auto& stmt: stmts) {
		// 	ASSERT_EQUAL(
		// 		stmt->getKind(),
		// 		query::entryPoint<compiler::helios::QuerySymbolOfSTMT>(
		// 			compiler::helios::KeyOf_QuerySymbolOfSTMT{ .stmt = stmt.borrow(), .scope = pst.
		// }
		// 		)
		// 	);
		// }
	}

	void testI32Consts() {
		auto constants_module
			= query::entryPoint<compiler::frontend::QueryModuleTree>(fs::FilePath(path("constants"))
		    );

		auto root_scope = query::entryPoint<compiler::helios::QueryRootScopeOf>(constants_module);

		auto symbols_in_module
			= query::entryPoint<compiler::helios::QuerySymbolsInScope>(root_scope);

		auto get_symbol = [&](auto&& name) {
			auto&& q
				= query::entryPoint<compiler::helios::QueryLookupInScope>({ name,
			                                                                base::StrId(name) });
			return q.getAsSingle();
		};

		// auto get_value =
		// 	[&](auto&& name) {
		// 		return query::entryPoint<QueryLookupConstValueInScope>();
		// 	}
		//
		// auto C
		// 	= get_symbol("C");
		// ASSERT_EQUAL(1, C->requestValue().getData<i32>().back() == 1);
		//
		// auto A = get_symbol("A");
		// ASSERT_EQUAL(1, A->requestValue().getData<i32>().back());
		//
		// auto B = get_symbol("B");
		// ASSERT_EQUAL(-3, B->requestValue().getData<i32>().back());
		//
		// auto D = get_symbol("D");
		// ASSERT_EQUAL(-1, D->requestValue().getData<i32>().back());
		//
		// auto H2 = get_symbol("H2");
		// ASSERT_EQUAL(3, H2->requestValue().getData<i32>().back());
		//
		// auto T0 = get_symbol("T0");
		// auto T1 = get_symbol("T1");
		// auto T2 = get_symbol("T2");
		// ASSERT_EQUAL(1, T0->requestValue().getData<i32>().back() == 1);
		// ASSERT_EQUAL(2, T1->requestValue().getData<i32>().back() == 2);
		// ASSERT_EQUAL(3, T2->requestValue().getData<i32>().back() == 3);
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
