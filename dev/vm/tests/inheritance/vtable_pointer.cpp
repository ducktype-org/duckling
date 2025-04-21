#include <vm_tester_utils.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/builders/errors.hpp>

class VTablePtrTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VTablePtrTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(vtablePtrExistance); }


private:
	void vtablePtrExistance() {
		using namespace vm::code;
		using namespace vm::code::builders;
		TypeContextBuilder type_context_builder{};

		DataType::VTable vtable{
			.kind            = DataType::VTable::Interface{},
			.implements      = {},
			.virtual_methods = {},
		};
		DataType without_vtable_ptr(base::StrID("myInterface"), {}, vtable);

		assertThrows<MissingVTablePtrError>(
			[&] { type_context_builder.addType(without_vtable_ptr); },
			"Classes and interfaces require having VTablePtr as their first field"
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/inheritance/");
