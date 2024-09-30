#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <tester/tester.hpp>
#include <typesystem/higher/typesystem.hpp>

#include <base/variant.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <query_framework/test_utils/context_suite.hpp>

using namespace tsh;
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
		const compiler::helios::SymID my_class_symbol = getChain("MyClass", root_scope).back();

		const TypeInfo my_class_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(my_class_symbol).expect("Not expecting an ERROR here...");


		withContextDo([&](query::Context& ctx) {
			TypeInterface my_class_interface = my_class_type.getInterface(ctx);

			assertTrue(
				my_class_interface.getElements(base::StrID("a")).size() == 1,
				"There should be exactly one 'a' member."
			);
			assertTrue(
				my_class_interface.getElements(base::StrID("b")).size() == 1,
				"There should be exactly one 'b' member."
			);
			assertTrue(
				my_class_interface.getElements(base::StrID("c")).empty(),
				"There should be exactly no 'c' members."
			);

			auto a_resolution = my_class_interface.resolve(base::StrID("a"), ctx);

			variant_match(a_resolution) {
				variant_case_novalue(TypeInterface::SingleMatch) {}
				variant_default { assertTrue(false, "Member 'a' should match exactly."); }
			}

			assertTrue(
				my_class_type.getSize(ctx) == 64,
				"MyClass should have size equal to the sum of sizes of its members."
			);
		});
	}

public:
	~TypeSystemOverloadResolutionTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/typesystem/higher/tests/")
