#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>

#include <base/variant.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <query_framework/test_utils/context_suite.hpp>

using namespace ts;
using namespace compiler::helios::test_utils;

class TypeSystemOverloadResolutionTest final: public tester::ContextSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeSystemOverloadResolutionTest

public:
	TypeSystemOverloadResolutionTest(tester::TestConfig&& config):
		  tester::ContextSuite(std::move(config), "TypeSystem overload resolution test") {
		TESTER_ADD_TEST(overload_resolution_test);
	}

private:
	void overload_resolution_test() {
		auto [_, root_scope] = getModule(fs::FilePath(path("class_definitions")));
		const compiler::helios::SymID my_struct_symbol = getChain("MyStruct", root_scope).back();
		const TypeInfo                my_struct_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(my_struct_symbol);

		withContext([&](query::Context& ctx) {
			TypeInterface my_struct_interface = my_struct_type.getInterface(ctx);

			assert(
				my_struct_interface.getElements(base::StrId("a")).size() == 1,
				"There should be exactly one 'a' member."
			);
			assert(
				my_struct_interface.getElements(base::StrId("b")).size() == 1,
				"There should be exactly one 'b' member."
			);
			assert(
				my_struct_interface.getElements(base::StrId("c")).empty(),
				"There should be exactly no 'c' members."
			);

			auto a_resolution = my_struct_interface.resolve(base::StrId("a"), ctx);

			variant_match(a_resolution) {
				variant_case_novalue(TypeInterface::SingleMatch) {}
				variant_default { assert(false, "Member 'a' should match exactly."); }
			}

			assert(
				my_struct_type.getSize(ctx) == 64,
				"MyStruct should have size equal to the sum of sizes of its members."
			);

			return nullptr;
		});
	}

public:
	~TypeSystemOverloadResolutionTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/typesystem/tests/")
