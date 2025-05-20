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
		TESTER_ADD_TEST(downcast);
		TESTER_ADD_TEST(upcast);
		// TESTER_ADD_TEST(dynamicDispatch); // @TODO shortly.
		TESTER_ADD_TEST(semantics);
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
		runTestOnVm("semantics/dynamic_dispatch.dbc", "", "", {}, 0);
		runTestOnVm("semantics/deep_dynamic_dispatch.dbc", "", "", {}, 0);
		runTestOnVm("semantics/dynamic_call_with_args.dbc", "", "", {}, 0);
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
