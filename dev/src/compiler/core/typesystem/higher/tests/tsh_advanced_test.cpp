#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <query_frameworkcontext/context.hpp>
#include <query_frameworkentry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using query::utils::withContextDo;

class TypeSystemClassFieldsTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeSystemClassFieldsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(classInterfaceTest); }

private:
	void classInterfaceTest() {
		auto [_, root_scope] = getModule(fs::File(path("class_definitions")));
		const compiler::helios::SymID my_class_symbol = getChain("MyClass", root_scope).back();

		const AbstractType my_class_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(my_class_symbol)
		          ->valueOrPanicMsg("Not expecting an ERROR here...")
		          .getType();


		withContextDo([&](query::Context& ctx) {
			CRef my_class_interface = my_class_type.getInterface(ctx);

			// note: If the type interface is modified,
			// these values might need to be updated.

			assertTrue(
				my_class_interface->getElements().size() == 6, "There should be exactly six members."
			);
			assertTrue(
				my_class_interface->getElementsByName().size() == 4,
				"There should be exactly four unique names."
			);

			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("a")).size() == 1,
				"There should be exactly one 'a' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("b")).size() == 1,
				"There should be exactly one 'b' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("c")).empty(),
				"There should be exactly no 'c' members."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("getA")).size() == 1,
				"There should be exactly one 'getA' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("add")).size() == 3,
				"There should be exactly three 'add' members."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("nonExistent")).empty(),
				"There should be exactly no 'nonExistent' members."
			);
		});
	}


public:
	~TypeSystemClassFieldsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/typesystem/higher/tests/")
