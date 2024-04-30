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

		auto get_value = [&](auto&& name) {
			return query::queryEntryPoint<compiler::helios::QueryConstValueOf>(get_chain(name).back(
			));
		};
		//
		ASSERT_EQUAL(1, get_value("N.X"));
		ASSERT_EQUAL(1, get_value("A"));
		ASSERT_EQUAL(-3, get_value("B"));
		ASSERT_EQUAL(-1, get_value("D"));
		ASSERT_EQUAL(6, get_value("E"));
		ASSERT_EQUAL(std::numeric_limits<i32>::max(), get_value("MAX_I32"));
		ASSERT_EQUAL(3, get_value("H2"));
		ASSERT_EQUAL(1, get_value("T0"));
		ASSERT_EQUAL(2, get_value("T1"));
		ASSERT_EQUAL(3, get_value("T2"));
		ASSERT_EQUAL(75, get_value("F"));
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
