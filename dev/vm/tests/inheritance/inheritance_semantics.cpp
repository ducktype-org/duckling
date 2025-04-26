#include <vm_tester_utils.hpp>

#include <base/optional.hpp>
#include <base/variant.hpp>

#include <vm/api/api.hpp>
#include <vm/bytecode/builders/errors.hpp>

class VmInheritanceSemanticsTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmInheritanceSemanticsTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(semantics); }


private:
	void semantics() {
		runTestOnVm("valid_upcast.dbc", {}, {}, {}, 0);
		runTestOnVm("downcast.dbc", "", "10", {}, 0);

		// Invalid
		using namespace vm::code::builders;
		auto filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{ "invalid_instantiation.dbc", UninstantiableValue::ERR_MSG },
			{ "missing_ext.dbc", InvalidInstructionExtension::ERR_MSG },
			// { "invalid_upcast.dbc", InvalidUpcast::ERR_MSG }, // @TODO after #732
		});

		for (auto& [filename, error]: filename_and_error) loadInvalidDbc(filename, { error });
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
