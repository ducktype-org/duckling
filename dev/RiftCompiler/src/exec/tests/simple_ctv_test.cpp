#include <exec/ctv.hpp>
#include <iostream>
#include <operations/create_default.hpp>
#include <operations/operation.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>
#include <base/string_id.hpp>

class SimpleExecTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleExecTest
public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Exec Test") {
		TESTER_ADD_TEST(simple);
		TESTER_ADD_TEST(class_subCTV_test);
	}

private:
	void simple() {
		ts::TypeDesc<> td(ts::IntegralInfo::create(8));

		for (int i = 0; i < 4; i++) {
			exec::alloc_new(td, 8);
		}

		exec::CTV ctv = exec::alloc_new(td, 8);

		assert(ctv.getData().size() * 8 == 8, "Size of ctv is incorrect");

		ctv.getData()[0] = 2;


		auto p_ctv = ctv.makePointer();

		auto pointer = ((uint32_t*)p_ctv.getData().data());
		auto block = pointer[0];
		auto offset = pointer[1];

		// or:
		auto pointer_data = p_ctv.getData<uint32_t>();
		auto block_ = pointer_data[0];
		auto offset_ = pointer_data[1];

		assert(block_ == block && offset_ == offset, "Wrapper for data access didn't work");

		assert(block == ctv.data.block, "Pointer has incorrect block");

		assert(offset == ctv.data.offset, "Pointer has incorrect offset");

		assert(p_ctv.getData<uint32_t>().size() == 2,
		       "Pointer doesn't hold two values (block id and offset)");


		assert(exec::getBlocks()[block].data[0] == 2, "Incorrect value under pointer.");

		ctv.getData()[0] = 4;

		assert(exec::getBlocks()[block].data[0] == 4, "Incorrect value under pointer.");

		auto under = p_ctv.getDataUnderPointer();

		assert(under[0] == 4, "Value given by pointer (by data_under_pointer) is wrong");
	}

	void class_subCTV_test() {

		ts::TypeDesc<> int_desc(ts::IntegralInfo::create(8));
		auto symbol0 = symtable::SymbolId::next();

		ts::ClassInfo parent_class(
			ts::ClassInfo::create(base::StrId("Parent"), {{int_desc, symbol0}}));
		ts::TypeDesc<> desc_parent_class(parent_class);

		auto symbol1 = symtable::SymbolId::next();

		ts::ClassInfo inheriting_class(ts::ClassInfo::create(
			base::StrId("Inheriting"),
			{{int_desc, symbol1}},
			{{parent_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0));
		ts::TypeDesc<> desc_inheriting_class(inheriting_class);

		exec::CTV classCTV = exec::alloc_new(desc_inheriting_class, inheriting_class.getSize());

		assert(inheriting_class.getMemberInfo(symbol0).start_offset.has_value(),
		       "Symbol0 not found in the class");

		size_t symbol0_offset = inheriting_class.getMemberInfo(symbol0).start_offset.value();

		classCTV.getData()[symbol0_offset / ts::BYTE_SIZE] = 123;

		assert(inheriting_class.getMemberInfo(symbol1).isOk(), "Symbol1 not found in the class");

		size_t symbol1_offset = inheriting_class.getMemberInfo(symbol1).start_offset.value();

		classCTV.getData()[symbol1_offset / ts::BYTE_SIZE] = 210;

		exec::CTV s0CTV = classCTV.subCTV(int_desc, symbol0_offset, int_desc.getType().getSize());
		assert(s0CTV.getData()[0] == 123, "Wrong value in the member subCTV");

		exec::CTV s1CTV = classCTV.subCTV(int_desc, symbol1_offset, int_desc.getType().getSize());
		assert(s1CTV.getData()[0] == 210, "Wrong value in the parent's member subCTV");

		assert(inheriting_class.getAncestorInfo(parent_class).isOk(), "Parent not found in class");
		size_t parent_offset = inheriting_class.getAncestorInfo(parent_class).start_offset.value();

		exec::CTV parentCTV =
			classCTV.subCTV(desc_parent_class, parent_offset, parent_class.getSize());

		assert(parent_class.getMemberInfo(symbol0).isOk(), "Parent didn't have symbol0");
		symbol0_offset = parent_class.getMemberInfo(symbol0).start_offset.value();

		s0CTV = parentCTV.subCTV(int_desc, symbol0_offset, int_desc.getType().getSize());
		assert(s0CTV.getData()[0] == 123,
		       "Wrong value in the parent's member subCTV when getting there indirectly");
	}

public:
	~SimpleExecTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/exec/tests/")
