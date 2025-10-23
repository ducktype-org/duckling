#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <typesystem/higher/all.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context_fd.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace tsh;
using namespace compiler::helios::test_utils;
using query::utils::withContextDo;

class TypeSystemOverloadResolutionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeSystemOverloadResolutionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(overloadResolutionTest); }

private:
	void overloadResolutionTest() {
		auto [_, root_scope] = getModule(fs::File(path("class_definitions")));
		const compiler::helios::SymID my_class_symbol = getChain("MyClass", root_scope).back();

		const AbstractType my_class_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(my_class_symbol)
		          ->expect("Not expecting an ERROR here...")
		          .getType();


		withContextDo([&](query::Context& ctx) {
			const TypeInterface& my_class_interface = my_class_type.getInterface(ctx);

			assertTrue(
				my_class_interface.getElementsWithName(base::StrID("a")).size() == 1,
				"There should be exactly one 'a' member."
			);
			assertTrue(
				my_class_interface.getElementsWithName(base::StrID("b")).size() == 1,
				"There should be exactly one 'b' member."
			);
			assertTrue(
				my_class_interface.getElementsWithName(base::StrID("c")).empty(),
				"There should be exactly no 'c' members."
			);

			auto a_resolution = my_class_interface.resolve(base::StrID("a"), ctx);

			variant_match(a_resolution) {
				variant_case_novalue(TypeInterface::SingleMatch) {}
				variant_default { assertTrue(false, "Member 'a' should match exactly."); }
			}
		});
	}

public:
	~TypeSystemOverloadResolutionTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/typesystem/higher/tests/")
