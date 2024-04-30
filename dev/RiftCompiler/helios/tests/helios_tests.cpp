#include "base/str_utils.hpp"
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

		TESTER_ADD_TEST(testI32Consts);
	}

private:
	std::pair<pst::PST, const fs::FilePath&> prepare(const fs::FilePath& file) {
		auto td     = lexer::tokenizeFile(file);
		auto parsed = pst::parse(std::move(td));
		assert(parsed.getLogger().good(), "there are unexpected errors in rift source-code");
		return { std::move(parsed), file };
	}

	void testI32Consts() {
		auto constants_module = query::queryEntryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("constants"))
		);

		auto root_scope
			= query::queryEntryPoint<compiler::helios::QueryRootScopeOf>(constants_module);

		auto symbols_in_module
			= query::queryEntryPoint<compiler::helios::QuerySymbolsInScope>(root_scope);

		auto get_symbol = [&](const std::string& name) {
			auto&& q = query::queryEntryPoint<compiler::helios::QueryLookupInScopeAndParents>(
				{ root_scope, base::StrId(name.c_str()), true }
			);
			ASSERT_EQUAL(true, q.isSingle());

			compiler::helios::SymbolList result;
			for (auto&& path = q.getAsSingle(); auto&& elem: path) {
				auto&& dealiased = query::queryEntryPoint<compiler::helios::QueryDealias>(elem);
				result.insert(result.end(), dealiased.begin(), dealiased.end());
			}

			return result;
		};

		auto get_chain = [&](auto chain) {
			auto                         symbols = base::split(chain, ".");
			compiler::helios::SymbolList result;
			bool                         first_symbol = true;
			for (auto&& sym: symbols) {
				auto symbol
					= first_symbol
				        ? query::queryEntryPoint<compiler::helios::QueryLookupInScopeAndParents>(
							{ root_scope, base::StrId(sym.c_str()), true }
						)
				        : query::queryEntryPoint<compiler::helios::QueryLookupInSymbol>({
							result.back(),
							base::StrId(sym.c_str()),
							false,
						});
				for (auto&& symbol_path = symbol.getAsSingle(); auto&& elem: symbol_path) {
					auto&& dealiased = query::queryEntryPoint<compiler::helios::QueryDealias>(elem);
					result.insert(result.end(), dealiased.begin(), dealiased.end());
				}
				first_symbol = false;
			}
			return result;
		};

		auto symb = get_symbol("X");
		std::cout << "Path: \n";
		for (auto&& sym_id: symb) std::cout << compiler::helios::name(sym_id).str() << '\n';

		auto absolute_path = get_chain("NN.A");
		std::cout << "Path: \n";
		for (auto&& sym_id: absolute_path)
			std::cout << compiler::helios::name(sym_id).str() << '\n';

		// auto get_value =
		// 	[&](auto&& name) {
		// 		return query::queryEntryPoint<QueryLookupConstValueInScope>();
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
