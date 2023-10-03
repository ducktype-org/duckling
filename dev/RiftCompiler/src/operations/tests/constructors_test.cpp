#include <base/string_id.hpp>
#include <exec/ctv.hpp>
#include <operations/constructors.hpp>
#include <operations/operation.hpp>
#include <tester/tester.hpp>
#include <typesystem/types.hpp>
#include <typesystem/typesystem.hpp>

class OperationsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OperationsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple Operation Test") {
		exec::init();
		TESTER_ADD_TEST(simple_constructor);
	}

private:
	void simple_constructor() {
		auto class_info = ts::ClassInfo::create(base::StrId("class"), {});
		// operation::Constructor cons {{}, {}, {}};

		operation::Constructor cons = operation::makeConstructorClass(class_info);

		exec::CTV ctv = exec::alloc_new(class_info);
		operation::execConstructor(cons, {}, ctv);

		ts::TypeDesc<> int_desc(ts::IntegralInfo::create(8));
		auto           symbol0 = symtable::SymbolId::next();

		ts::ClassInfo parent_class(
			ts::ClassInfo::create(base::StrId("parent"), { { int_desc, symbol0 } })
		);
		ts::TypeDesc<> desc_parent_class(parent_class);

		auto symbol1 = symtable::SymbolId::next();

		ts::ClassInfo  inheriting_class(ts::ClassInfo::create(
            base::StrId("inheriting"),
            { { int_desc, symbol1 } },
            { { parent_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
            0
        ));
		ts::TypeDesc<> desc_inheriting_class(inheriting_class);

		operation::Constructor cons_parent = operation::makeConstructorClass(parent_class);

		auto ctv_parent = exec::alloc_new(parent_class);

		auto ctv_int = exec::alloc_new(int_desc.getType());
		operation::getDefault(operation::Defaultable::ConstructEmpty, int_desc.getType())(
			{ ctv_int }
		);
		ctv_int.getData<int8_t>().front() = 14;

		message(base::strConcat("there are ", (u64) cons_parent.operations.size(), "operations"));

		operation::execConstructor(cons_parent, { ctv_int }, ctv_parent);

		auto x = exec::getMember(ctv_parent, parent_class.getMemberInfo(symbol0), parent_class);

		assert(
			x.getData<int8_t>().front() == 14,
			base::strConcat(
				"This member should be set to ",
				(u64) (14),
				" but is ",
				(u64) (x.getData<int8_t>().front())
			)
		);

		ctv_int.getData<int8_t>().front() = 28;

		assert(
			x.getData<int8_t>().front() == 14,
			base::strConcat(
				"This member should be set to ",
				(u64) (14),
				" but is ",
				(u64) (x.getData<int8_t>().front())
			)
		);

		operation::Constructor cons_inherit = operation::makeConstructorClass(inheriting_class);
		auto                   ctv_inherit  = exec::alloc_new(inheriting_class);

		// operation::execConstructor(cons_inherit, {ctv_int, ctv_parent},
		// ctv_inherit);
	}

public:
	~OperationsTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/operations/tests/")
