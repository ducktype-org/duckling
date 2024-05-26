#include <base/str_utils.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <helios/ts/ts.hpp>
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
		TESTER_ADD_TEST(testEdgeEvals);
	}

private:
	static auto getChain(auto chain, auto scope) {
		auto                         symbols = base::strSplit(chain, ".");
		compiler::helios::SymbolList result;
		bool                         first_symbol = true;
		for (auto&& sym: symbols) {
			auto symbol = first_symbol
			                ? query::entryPoint<compiler::helios::QueryLookupInScopeAndParents>(
								{ scope, base::StrId(sym.c_str()), true }
							)
			                : query::entryPoint<compiler::helios::QueryLookupInSymbol>(
								{ result.back(), base::StrId(sym.c_str()), false }
							);
			for (auto&& symbol_path = symbol.getAsSingle(); auto&& elem: symbol_path) {
				auto dealiased = query::entryPoint<compiler::helios::QueryDealias>(elem);
				result.insert(result.end(), dealiased.begin(), dealiased.end());
			}
			first_symbol = false;
		}
		return result;
	}

	static auto getValue(auto name, auto scope) {
		return query::entryPoint<compiler::helios::QueryConstValueOf>(getChain(name, scope).back());
	}

	void testI32Consts() {
		auto constants_module = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/constants"))
		);

		auto root_scope = query::entryPoint<compiler::helios::QueryRootScopeOf>(constants_module);

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

		auto get_value = [&](auto name) {
			return query::entryPoint<compiler::helios::QueryConstValueOf>(get_chain(name).back());
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

		// @TODO: Move this to new test testTypeOf().
		auto get_type_of = [&](auto name) {
			return query::entryPoint<compiler::helios::QueryTypeOf>(get_chain(name).back());
		};

		auto INT32_TYPE = query::entryPoint<ts::QueryIntegralType>({ 32, true });
		ASSERT_EQUAL(true, INT32_TYPE == get_type_of("T0"));
	}

	void testEdgeEvals() {
		auto edge_evals = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/edge_evals"))
		);
		auto root_scope = query::entryPoint<compiler::helios::QueryRootScopeOf>(edge_evals);
		ASSERT_EQUAL(1, getValue("M1", root_scope));
		ASSERT_EQUAL(6, getValue("M2", root_scope));
		ASSERT_EQUAL(7, getValue("O1", root_scope));
		ASSERT_EQUAL(7, getValue("O2", root_scope));
		ASSERT_EQUAL(7, getValue("O3", root_scope));
		ASSERT_EQUAL(7, getValue("O4", root_scope));
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
