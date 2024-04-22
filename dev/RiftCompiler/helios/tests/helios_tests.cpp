#include "helios/scope_symbol_id.hpp"
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
		auto [pst, file]  = prepare(fs::FilePath(path("constants.rift")));
		auto   root_scope = pst.getTopLevelElement();
		auto&& stmts      = root_scope->getStatements();
		// @TODO: Write a QuerySymbolOfSTMT test here.
		// EDIT: I don't know how to construct ScopeID for QuerySymbolOfSTMT key...
		// for (const auto& stmt: stmts) {
		// 	ASSERT_EQUAL(
		// 		stmt->getKind(),
		// 		query::queryEntryPoint<compiler::helios::QuerySymbolOfSTMT>(
		// 			compiler::helios::KeyOf_QuerySymbolOfSTMT{ .stmt = stmt.borrow(), .scope = pst.
		// }
		// 		)
		// 	);
		// }
	}

	// @NEARFUTURE: This can be adapted to use HELIOS
	// void testI32Consts() {
	// 	hir::HIR hir;
	// 	{
	// 		auto source = prepare(fs::FilePath(path("constants.rift")));
	// 		hir.addUnit(std::move(source));
	// 	}
	// 	hir.doMagicStuff();
	//
	// 	auto root_scope = hir.getState().symTable().getRootScope();
	// 	assert(
	// 		root_scope->getSymbols().size() == 1, "Root scope does not has single sub root scope"
	// 	);
	//
	// 	auto top_level_symbol = root_scope->getSymbols()[0];
	//
	// 	auto get_symbol_from_top_level
	// 		= [&](auto name) { return getSymbolFromLookupIn(top_level_symbol, base::StrId(name)); };
	//
	// 	auto C = get_symbol_from_top_level("C");
	// 	assert(C->requestValue().getData<i32>().back() == 1, "Bad value of C");
	//
	// 	auto A = get_symbol_from_top_level("A");
	// 	assert(A->requestValue().getData<i32>().back() == 1, "Bad value of A");
	//
	// 	auto B = get_symbol_from_top_level("B");
	// 	assert(B->requestValue().getData<i32>().back() == -3, "Bad value of B");
	//
	// 	auto D = get_symbol_from_top_level("D");
	// 	assert(D->requestValue().getData<i32>().back() == -1, "Bad value of D");
	//
	// 	auto H2 = get_symbol_from_top_level("H2");
	// 	assert(H2->requestValue().getData<i32>().back() == 3, "Bad value of H2");
	//
	// 	auto T0 = get_symbol_from_top_level("T0");
	// 	auto T1 = get_symbol_from_top_level("T1");
	// 	auto T2 = get_symbol_from_top_level("T2");
	// 	assert(T0->requestValue().getData<i32>().back() == 1, "Bad value of T0");
	// 	assert(T1->requestValue().getData<i32>().back() == 2, "Bad value of T1");
	// 	assert(T2->requestValue().getData<i32>().back() == 3, "Bad value of T2");
	// }
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
