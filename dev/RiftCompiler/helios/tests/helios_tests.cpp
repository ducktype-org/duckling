#include <base/str_utils.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>
#include <query_framework/query_entry_point.hpp>
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
		TESTER_ADD_TEST(testEdgeEvals);
		TESTER_ADD_TEST(testTypeOf);
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

	static auto getTypeOf(auto name, auto scope) {
		return query::entryPoint<compiler::helios::QueryTypeOf>(getChain(name, scope).back());
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
	}

	void testStructInfo() {

	}

	void testTypeOf() {
		auto structs_and_types_modules = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/structs_and_types"))
		);

		auto root_scope
			= query::entryPoint<compiler::helios::QueryRootScopeOf>(structs_and_types_modules);

		auto INT32_TYPE = query::entryPoint<ts::QueryIntegralType>({ 32, true });
		auto F32_TYPE   = query::entryPoint<ts::QueryFloatType>(32);

		ASSERT_EQUAL(true, INT32_TYPE == getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(true, F32_TYPE == getTypeOf("SimpleFloat", root_scope));
		auto&& first_struct = getChain("FirstStructEver", root_scope);
		auto&& first_struct_info
			= query::entryPoint<compiler::helios::QueryStructInfo>(first_struct.back());
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
