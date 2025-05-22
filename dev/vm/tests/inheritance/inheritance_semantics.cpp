#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/builders/errors.hpp>

class VmInheritanceSemanticsTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceSemanticsTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// TESTER_ADD_TEST(downcast);
		// TESTER_ADD_TEST(upcast);
		TESTER_ADD_TEST(dynamicDispatch);
		// TESTER_ADD_TEST(semantics);
	}


private:
	void downcast() { runTestOnVm("semantics/downcast.dbc", "", "10", {}, 0); }

	void upcast() {
		runTestOnVm("semantics/valid_upcast.dbc", {}, {}, {}, 0);
		loadInvalidDbc(
			"semantics/invalid_upcast.dbc", { vm::code::builders::InvalidUpcastError::ERR_MSG }
		);
	}

	void dynamicDispatch() {
		// runTestOnVm("semantics/error.dbc", "", "420420", {}, 0);
		runTestOnVm("semantics/dynamic_call_with_args.dbc", "12 13", "25", {}, 0);
		// runTestOnVm("semantics/very_simple_dispatch.dbc", "", "420", {}, 0);
		// runTestOnVm("semantics/nested_simple_dispatch_1.dbc", "", "2137", {}, 0);
		// runTestOnVm("semantics/nested_simple_dispatch_2.dbc", "", "2137", {}, 0);
		// runTestOnVm("semantics/dynamic_dispatch_1.dbc", "", "0", {}, 0);
		// runTestOnVm("semantics/dynamic_dispatch_2.dbc", "", "2", {}, 0);
		// runTestOnVm("semantics/dynamic_dispatch_3.dbc", "", "3", {}, 0);
		// runTestOnVm("semantics/dynamic_dispatch_4.dbc", "", "4", {}, 0);
		// runTestOnVm("semantics/dynamic_dispatch_all.dbc", "", "0 2 3 4", {}, 0);
		// runTestOnVm("semantics/deep_dynamic_dispatch.dbc", "", "", {}, 0);
		// runTestOnVm("semantics/dynamic_call_with_args.dbc", "", "", {}, 0);
	}

	void semantics() {
		using namespace vm::code::builders;
		auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{ "semantics/invalid_instantiation.dbc", UninstantiableValueError::ERR_MSG },
			{ "semantics/missing_ext.dbc", InvalidInstructionExtensionError::ERR_MSG },
		});

		for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
