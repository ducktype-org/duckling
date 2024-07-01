#include <base/str_utils.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/scopes/scopes.hpp>
#include <helios/symbols/symbols.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <pst_parser/parser.hpp>
#include <filesystem/file.hpp>
#include <helios/queries.hpp>
#include <lexer/lexer.hpp>
#include <typesystem/typesystem.hpp>
#include <typesystem/internal/queries.hpp>

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
		TESTER_ADD_TEST(testStructInfo);
		TESTER_ADD_TEST(testTypeOf);
		TESTER_ADD_TEST(simpleHOUTTest);
		TESTER_ADD_TEST(importTest);
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

	static auto getTypeFromDefintion(auto name, auto scope) {
		return query::entryPoint<compiler::helios::QueryTypeFromDefinition>(
			getChain(name, scope).back()
		);
	}

	std::pair<compiler::frontend::ModuleId, compiler::helios::ScopeID>
		getModule(const std::string& name) {
		auto&& constants_module = query::entryPoint<compiler::frontend::QueryModuleTree>(
			fs::FilePath(path("test_modules/" + name))
		);

		auto&& root_scope = query::entryPoint<compiler::helios::QueryRootScopeOf>(constants_module);
		return { constants_module, root_scope };
	}

	void testI32Consts() {
		auto [_, root_scope] = getModule("constants");

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
		auto [_, root_scope] = getModule("structs");

		const auto first_struct = getChain("FirstStructEver", root_scope).back();
		const auto first_struct_info
			= query::entryPoint<compiler::helios::QueryStructInfo>(first_struct);
		const auto first_struct_typeinfo
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(first_struct);

		ASSERT_EQUAL(2, first_struct_info.members.size());
		ASSERT_EQUAL(2, first_struct_info.methods.size());
		ASSERT_EQUAL(0, first_struct_info.bases.size());
		ASSERT_EQUAL("FirstStructEver", first_struct_info.name);

		const auto second_struct = getChain("SecondStruct", root_scope).back();
		auto       second_struct_info
			= query::entryPoint<compiler::helios::QueryStructInfo>(second_struct);

		ASSERT_EQUAL(0, second_struct_info.members.size());
		ASSERT_EQUAL(0, second_struct_info.methods.size());
		ASSERT_EQUAL(1, second_struct_info.bases.size());
		ASSERT_EQUAL(true, first_struct_typeinfo == second_struct_info.bases.front());
		ASSERT_EQUAL("SecondStruct", second_struct_info.name);
	}

	void testTypeOf() {
		auto [_, root_scope] = getModule("types");

		const auto INT32_TYPE = query::entryPoint<ts::QueryIntegralType>({ 32, true });
		const auto F32_TYPE   = query::entryPoint<ts::QueryFloatType>(32);

		ASSERT_EQUAL(true, INT32_TYPE == getTypeOf("SimpleInt", root_scope));
		ASSERT_EQUAL(true, F32_TYPE == getTypeOf("SimpleFloat", root_scope));

		const auto tuple_int_int           = getTypeOf("TupleII", root_scope);
		const auto tuple_int_int_type_info = query::entryPoint<ts::QueryTupleType>(
			{ { { INT32_TYPE, false }, { INT32_TYPE, false } } }
		);
		ASSERT_EQUAL(true, tuple_int_int == tuple_int_int_type_info);

		const auto first_variant = getTypeOf("first_variant", root_scope);
		const auto first_variant_type_info
			= query::entryPoint<ts::QueryVariantType>({ { INT32_TYPE, F32_TYPE } });
		ASSERT_EQUAL(true, first_variant == first_variant_type_info);

		const auto weird_variant = getTypeOf("weird_variant", root_scope);

		const auto structA     = getTypeFromDefintion("A", root_scope);
		const auto structB     = getTypeFromDefintion("B", root_scope);
		const auto structC     = getTypeFromDefintion("C", root_scope);
		auto       right_tuple = query::entryPoint<ts::QueryTupleType>(
            { { { structA, false },
		              { query::entryPoint<ts::QueryVariantType>({ { structB, structC } }), false } } }
        );
		const auto weird_variant_type
			= query::entryPoint<ts::QueryVariantType>({ { structA, right_tuple } });
		ASSERT_EQUAL(true, weird_variant == weird_variant_type);
	}

	void testEdgeEvals() {
		auto [_, root_scope] = getModule("edge_evals");

		ASSERT_EQUAL(1, getValue("M1", root_scope));
		ASSERT_EQUAL(6, getValue("M2", root_scope));
		ASSERT_EQUAL(7, getValue("O1", root_scope));
		ASSERT_EQUAL(7, getValue("O2", root_scope));
		ASSERT_EQUAL(7, getValue("O3", root_scope));
		ASSERT_EQUAL(7, getValue("O4", root_scope));
	}

	void simpleHOUTTest() {
		auto [module, _] = getModule("hout_simple_test");

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		ASSERT_EQUAL(hout.functions.size(), 3);
		ASSERT_EQUAL(hout.glob_data.size(), 2);
	}

	void importTest() {
		auto [module, _] = getModule("import_tests");

		auto hout = query::entryPoint<compiler::helios::QueryTopLevelEntities>(module);

		auto test_value = [&](auto str, i64 val) {
			auto name = base::StrId(str);
			for (auto& gb: hout.glob_data) {
				if (gb.original_name == name) {
					this->assert(gb.value == val, "Bad constant value");
					return;
				}
			}
			this->fail(base::strConcat("No constant of name: ", name.strView()));
		};

		test_value("sm1_v", 123'123);
		test_value("sm11_v", 7'812'313);
		test_value("it_through_alias", 19'923);
		test_value("sm1_through_sm11", 123'123);
		test_value("sm2_v", 777'666);
		test_value("cyclic_final", 6);
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/helios/tests/");
