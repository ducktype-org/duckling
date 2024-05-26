#include <exec/ctv.hpp>
#include <iostream>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>
#include <base/string_id.hpp>

class SimpleExecTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleExecTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Exec Test") { TESTER_ADD_TEST(simple); }

private:
	void simple() {
		ts::TypeDesc<> td(query::entryPoint<ts::QueryIntegralType>({ 8 }));

		for (i32 i = 0; i < 4; i++) exec::alloc_new(td, 8);

		exec::CTV ctv = exec::alloc_new(td, 8);

		assert(ctv.getData().size() * 8 == 8, "Size of ctv is incorrect");

		ctv.getData()[0] = 2;


		auto p_ctv = ctv.makePointer();

		auto pointer = reinterpret_cast<u32*>(p_ctv.getData().data());
		auto block   = pointer[0];
		auto offset  = pointer[1];

		// or:
		auto pointer_data = p_ctv.getData<u32>();
		auto block_       = pointer_data[0];
		auto offset_      = pointer_data[1];

		assert(block_ == block && offset_ == offset, "Wrapper for data access didn't work");

		assert(block == ctv.data.block, "Pointer has incorrect block");

		assert(offset == ctv.data.offset, "Pointer has incorrect offset");

		assert(
			p_ctv.getData<u32>().size() == 2,
			"Pointer doesn't hold two values (block id and offset)"
		);


		assert(exec::getBlocks()[block].data[0] == 2, "Incorrect value under pointer.");

		ctv.getData()[0] = 4;

		assert(exec::getBlocks()[block].data[0] == 4, "Incorrect value under pointer.");

		auto under = p_ctv.getDataUnderPointer();

		assert(under[0] == 4, "Value given by pointer (by data_under_pointer) is wrong");
	}

public:
	~SimpleExecTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/exec/tests/")
