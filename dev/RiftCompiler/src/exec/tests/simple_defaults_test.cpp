#include <exec/ctv.hpp>
#include <exec/exec.hpp>
#include <exec/helpers.hpp>
#include <exec/vtable_creation.hpp>


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
		exec::init();


		TESTER_ADD_TEST(default_equality_test);
		TESTER_ADD_TEST(default_comparison_test);
		TESTER_ADD_TEST(virtual_construct);
		TESTER_ADD_TEST(vtable_creation_test);
		TESTER_ADD_TEST(int_test);
		TESTER_ADD_TEST(class_test);
		TESTER_ADD_TEST(tuple_tests);
	}

private:
	void default_equality_test() {
		ts::ClassInfo parentClass = ts::ClassInfo::create(base::StrId("Parent"), {});
		auto parent_class_desc = ts::TypeDesc<>(parentClass);

		ts::ClassInfo memberClass = ts::ClassInfo::create(base::StrId("Member"), {});
		auto member_class_desc = ts::TypeDesc<>(memberClass);

		auto bool_desc = ts::TypeDesc<>(ts::BoolInfo::create());

		usize parent_counter = 0;
		exec::CTV parent_result_ctv = exec::alloc_new(bool_desc, bool_desc.getType().getSize());
		parent_result_ctv.getData<bool>().front() = true;

		usize member_counter = 0;
		exec::CTV member_result_ctv = exec::alloc_new(bool_desc, bool_desc.getType().getSize());
		member_result_ctv.getData<bool>().front() = true;

		auto fun = [&parent_counter,
		            &parent_result_ctv]([[maybe_unused]] const std::vector<exec::CTV>& input) {
			parent_counter++;
			return parent_result_ctv;
		};
		operation::TypedOperation parent_eq{
			fun, ts::FunctionInfo::create({parent_class_desc, parent_class_desc}, bool_desc)};
		operation::addDefault(operation::Defaultable::Equality, parentClass, parent_eq);

		auto fun2 = [&member_counter,
		             &member_result_ctv]([[maybe_unused]] const std::vector<exec::CTV>& input) {
			member_counter++;
			return member_result_ctv;
		};
		operation::TypedOperation member_eq{
			fun2, ts::FunctionInfo::create({member_class_desc, member_class_desc}, bool_desc)};
		operation::addDefault(operation::Defaultable::Equality, memberClass, member_eq);

		ts::ClassInfo customClass = ts::ClassInfo::create(
			base::StrId("custom"),
			{{member_class_desc, symtable::SymbolId::next()}},
			{{parentClass, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0);
		ts::TypeDesc<> custom_class_desc(customClass);

		auto x = operation::createDefaultEquality(customClass);

		exec::CTV a = exec::alloc_new(custom_class_desc, customClass.getSize());

		exec::CTV result = x({a, a});
		assert(
			result.getData<bool>().front(),
			"Parent comparison is supposed to return true but default comparison returned false");
		assert(parent_counter == 1, "Wrong number of compares on the parent class");
		assert(member_counter == 1, "Wrong number of compares on the member class");

		parent_result_ctv.getData<bool>().front() = false;

		result = x({a, a});
		assert(
			!result.getData<bool>().front(),
			"Parent comparison is supposed to return false but default comparison returned true");
		assert(parent_counter == 2, "Wrong number of compares on the parent class");
		assert(member_counter == 2, "Wrong number of compares on the member class");

		parent_result_ctv.getData<bool>().front() = true;
		member_result_ctv.getData<bool>().front() = false;

		result = x({a, a});
		assert(
			!result.getData<bool>().front(),
			"Member comparison is supposed to return false but default comparison returned true");
		assert(parent_counter == 2, "Wrong number of compares on the parent class");
		assert(member_counter == 3, "Wrong number of compares on the member class");

		parent_result_ctv.getData<bool>().front() = true;
		member_result_ctv.getData<bool>().front() = true;

		result = x({a, a});
		assert(result.getData<bool>().front(),
		       "Both results were set back to true but returned value is still false");
		assert(parent_counter == 3, "Wrong number of compares on the parent class");
		assert(member_counter == 4, "Wrong number of compares on the member class");
	}

	void default_comparison_test() {
		ts::ClassInfo parentClass = ts::ClassInfo::create(base::StrId("parent"), {});
		auto parent_class_desc = ts::TypeDesc<>(parentClass);

		ts::ClassInfo memberClass = ts::ClassInfo::create(base::StrId("member"), {});
		auto member_class_desc = ts::TypeDesc<>(memberClass);

		auto int_desc = ts::TypeDesc<>(ts::IntegralInfo::create(8));

		usize parent_counter = 0;
		exec::CTV parent_result_ctv = exec::alloc_new(int_desc, int_desc.getType().getSize());
		parent_result_ctv.getData<int8_t>().front() = 0;

		usize member_counter = 0;
		exec::CTV member_result_ctv = exec::alloc_new(int_desc, int_desc.getType().getSize());
		member_result_ctv.getData<int8_t>().front() = 0;

		auto fun = [&parent_counter,
		            &parent_result_ctv]([[maybe_unused]] const std::vector<exec::CTV>& input) {
			parent_counter++;
			return parent_result_ctv;
		};
		operation::TypedOperation parent_comp{
			fun, ts::FunctionInfo::create({parent_class_desc, parent_class_desc}, int_desc)};
		operation::addDefault(operation::Defaultable::Compare, parentClass, parent_comp);

		auto fun2 = [&member_counter,
		             &member_result_ctv]([[maybe_unused]] const std::vector<exec::CTV>& input) {
			member_counter++;
			return member_result_ctv;
		};
		operation::TypedOperation member_comp{
			fun2, ts::FunctionInfo::create({member_class_desc, member_class_desc}, int_desc)};
		operation::addDefault(operation::Defaultable::Compare, memberClass, member_comp);

		ts::ClassInfo customClass = ts::ClassInfo::create(
			base::StrId("custom"),
			{{member_class_desc, symtable::SymbolId::next()}},
			{{parentClass, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0);
		ts::TypeDesc<> custom_class_desc(customClass);

		auto x = operation::createDefaultComparison(customClass);

		exec::CTV a = exec::alloc_new(custom_class_desc, customClass.getSize());

		exec::CTV result = x({a, a});
		assert(result.getData<int8_t>().front() == 0,
		       "Parent comparison is supposed to return 0 but default comparison returned nonzero");
		assert(parent_counter == 1, "Wrong number of compares on the parent class");
		assert(member_counter == 1, "Wrong number of compares on the member class");

		parent_result_ctv.getData<int8_t>().front() = -1;

		result = x({a, a});
		assert(result.getData<int8_t>().front() == -1,
		       "Parent comparison is supposed to return -1 but default comparison returned ???");
		assert(parent_counter == 2, "Wrong number of compares on the parent class");
		assert(member_counter == 2, "Wrong number of compares on the member class");

		parent_result_ctv.getData<int8_t>().front() = 0;
		member_result_ctv.getData<int8_t>().front() = 1;

		result = x({a, a});
		assert(result.getData<int8_t>().front() == 1,
		       "Member comparison is supposed to return 1 but default comparison returned ???");
		assert(parent_counter == 2, "Wrong number of compares on the parent class");
		assert(member_counter == 3, "Wrong number of compares on the member class");

		parent_result_ctv.getData<int8_t>().front() = 1;
		member_result_ctv.getData<int8_t>().front() = 1;

		result = x({a, a});
		assert(result.getData<int8_t>().front() == 1,
		       "Both results were set back to 1 but returned value is still ???");
		assert(parent_counter == 2, "Wrong number of compares on the parent class");
		assert(member_counter == 4, "Wrong number of compares on the member class");
	}


	template<typename T, usize SIZE>
	void simple_int_test() {
		static_assert(SIZE == sizeof(T) * 8);
		// @TODO use the T and SIZE
		auto int_type = ts::IntegralInfo::create(SIZE);
		ts::TypeDesc<> int_desc{int_type};
		ts::TypeDesc<> bool_desc{ts::BoolInfo::create()};


		/*			INT TESTS 		*/


		exec::CTV int_ctv_a = exec::alloc_new(int_desc, int_type.getSize());
		exec::CTV int_ctv_b = exec::alloc_new(int_desc, int_type.getSize());

		int_ctv_a.getData<T>().front() = 42;
		int_ctv_b.getData<T>().front() = 27;

		auto eq_res = operation::getDefault(operation::Defaultable::Equality, int_type)({int_ctv_a, int_ctv_b});
		assert(eq_res.getData<bool>().front() == false, "42 should not be equal to 27.");


		auto cmp_res = operation::getDefault(operation::Defaultable::Compare, int_type)({int_ctv_a, int_ctv_b});
		assert(cmp_res.getData<int8_t>().front() == -1, "42 should be bigger than 27.");


		operation::getDefault(operation::Defaultable::Assign, int_type)({int_ctv_a, int_ctv_b});

		assert(int_ctv_a.getData<T>().front() == 27, "Assigment should set the value to 27");

		auto eq_res_1 = operation::getDefault(operation::Defaultable::Equality, int_type)({int_ctv_a, int_ctv_b});
		assert(eq_res_1.getData<bool>().front() == true, "After assigment values should be equal.");


		auto cmp_res_1 = operation::getDefault(operation::Defaultable::Compare, int_type)({int_ctv_a, int_ctv_b});
		assert(cmp_res_1.getData<int8_t>().front() == 0, "After assigment values should be equal.");
	}

	void int_test() {
		simple_int_test<int8_t, 8>();
		simple_int_test<i16, 16>();
		simple_int_test<i32, 32>();
		simple_int_test<i64, 64>();
		simple_int_test<i128, 128>();
	}

	void class_test() {
		auto int_type = ts::IntegralInfo::create(8);
		ts::TypeDesc<> int_desc{int_type};
		ts::TypeDesc<> bool_desc{ts::BoolInfo::create()};

		/*		CLASS SETUP */
		/*

		    struct A {
		        a : int8 = 27;
		    };

		    struct B : A {
		        b : int8 = 42;
		    }


		*/

		ts::ClassInfo class_A =
			ts::ClassInfo::create(base::StrId("A"), {{int_desc, symtable::SymbolId::next()}});

		auto b_member = symtable::SymbolId::next();

		ts::ClassInfo class_B = ts::ClassInfo::create(
			base::StrId("B"),
			{{int_desc, b_member}},
			{{class_A, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0);

		ts::TypeDesc<> class_A_desc{class_A};
		ts::TypeDesc<> class_B_desc{class_B};

		namespace op = operation;

		operation::TypedOperation class_A_compare =
			operation::createAddDefault(class_A, operation::Defaultable::Compare);

		operation::TypedOperation class_B_compare =
			op::createAddDefault(class_B, operation::Defaultable::Compare);

		operation::TypedOperation class_A_equality =
			op::createAddDefault(class_A, operation::Defaultable::Equality);

		operation::TypedOperation class_B_equality =
			op::createAddDefault(class_B, operation::Defaultable::Equality);

		operation::TypedOperation class_A_assign =
			op::createAddDefault(class_A, operation::Defaultable::Assign);

		operation::TypedOperation class_B_assign =
			op::createAddDefault(class_B, operation::Defaultable::Assign);


		operation::TypedOperation class_A_construct_full =
			op::createAddDefault(class_A, operation::Defaultable::ConstructFull);

		operation::TypedOperation class_B_construct_full =
			op::createAddDefault(class_B, operation::Defaultable::ConstructFull);

		operation::TypedOperation class_A_construct_empty{
			[](std::vector<exec::CTV> ctvs) {
				ctvs[0].getData<int8_t>().front() = 27;
				return ctvs[0];
			},
			ts::FunctionInfo::create({class_A_desc}, class_A_desc)};

		operation::addDefault(
			operation::Defaultable::ConstructEmpty, class_A, class_A_construct_empty);


		usize offset = 0;
		for (auto parent_data : class_B.basicParents()) {
			if (parent_data.info == class_A) {
				offset = parent_data.offset;
			}
		}

		operation::TypedOperation class_B_construct_empty{


			[class_B, class_A, offset, b_member](std::vector<exec::CTV> ctvs) {
				// @TODO: this code explicitly initializes A
			    // the code below uses default empty contructor of B for that
			    // This is shorter, but initialises B.b twice (once to zero, then to 42).
			    // Maybe those approaches should be somehow combined.

				// exec::CTV sub_A = ctvs[0].subCTV(class_A, offset, class_A.getSize());
			    // operation::getDefault(operation::Defaultable::ConstructEmpty,
			    // class_A).function({sub_A});

				op::createDefaultConstructEmpty(class_B)({ctvs[0]});
				exec::getMemberNonVirtual(ctvs[0], b_member).getData<int8_t>().front() = 42;

				return ctvs[0];
			},
			ts::FunctionInfo::create({class_B_desc}, class_B_desc)};

		operation::addDefault(
			operation::Defaultable::ConstructEmpty, class_B, class_B_construct_empty);


		auto b1 = exec::alloc_new(class_B_desc, class_B.getSize());

		operation::getDefault(operation::Defaultable::ConstructEmpty, class_B)({b1});

		message(base::strConcat("value b1.a: ", (usize)b1.getData<int8_t>()[0]));
		message(base::strConcat("value b1.b: ", (usize)b1.getData<int8_t>()[1]));


		assert(b1.getData<int8_t>()[0] == 27, "b1.a != 27");
		assert(b1.getData<int8_t>()[1] == 42, "b1.b != 42");


		auto a1 = exec::alloc_new(class_A_desc, class_A.getSize());
		operation::getDefault(operation::Defaultable::ConstructEmpty, class_A)({a1});
		assert(a1.getData<int8_t>().front() == 27, "a1.a != 27");


		a1.getData<int8_t>().front() = 20;

		auto int1 = exec::alloc_new(int_desc, int_type.getSize());
		int1.getData<int8_t>().front() = 80;

		auto int2 = exec::alloc_new(int_desc, int_type.getSize());
		int2.getData<int8_t>().front() = 10;


		auto b2 = exec::alloc_new(class_B_desc, class_B.getSize());


		std::cerr << "parameters:"
				  << operation::getDefault(operation::Defaultable::ConstructFull, class_A)
						 .signature.getParameterTypeList()
						 .size()
				  << "\n";

		operation::getDefault(operation::Defaultable::ConstructFull, class_A)({a1, int1});
		assert(a1.getData<int8_t>().front() == 80, "construct full didn't set a1.a to 80");


		operation::getDefault(operation::Defaultable::ConstructFull, class_B)({b2, int2, a1});

		std::cerr << "b2.getData<int8_t>()[0]  " << (int)b2.getData<int8_t>()[0]
				  << ",b2.getData<int8_t>()[1]  " << (int)b2.getData<int8_t>()[1] << "\n";

		assert(b2.getData<int8_t>()[0] == 80, "construct full didn't set b2.a to 80");
		assert(b2.getData<int8_t>()[1] == 10, "construct full didn't set b2.b to 10");
	}


	void virtual_construct() {
		namespace op = operation;
		using enum op::Defaultable;
		/*
		        Z			Z
		        |v			|
		        A			B
		*/

		ts::TypeDesc<> int_desc(ts::IntegralInfo::create(8));
		auto symbol_z = symtable::SymbolId::next();
		auto Z = ts::ClassInfo::create(base::StrId("Z"), {{int_desc, symbol_z}});

		operation::Operation construct_z = [](const std::vector<exec::CTV>& ctvs) {
			ctvs[0].getData<uint8_t>().front() = 7;
			return ctvs[0];
		};

		operation::TypedOperation construct_z_t{construct_z, ts::FunctionInfo::create({Z}, Z)};

		operation::addDefault(operation::Defaultable::ConstructEmpty, Z, construct_z_t);


		auto ctv_z = exec::alloc_new(Z);
		auto cons = operation::getDefault(ConstructEmpty, Z);

		cons({ctv_z});


		assert(ctv_z.getData<uint8_t>().front() == 7, "Empty constructor didn't set the value");


		auto B = ts::ClassInfo::create(
			base::StrId("B"),
			{},
			{{Z, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0);


		auto ctv_b = exec::alloc_new(B);

		op::createAddDefault(B, ConstructEmpty);

		operation::getDefault(ConstructEmpty, B)({ctv_b});

		auto member_b = getMemberNonVirtual(ctv_b, symbol_z);

		assert(member_b.getData<uint8_t>().front() == 7,
		       base::strConcat("Constructor of B didn't set field in parent ",
		                        (u64)member_b.getData<uint8_t>().front()));


		auto A =
			ts::ClassInfo::create(base::StrId("A"),
		                          {},
		                          {{Z, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public)}},
		                          0);


		auto ctv_a = exec::alloc_new(A);

		op::createAddDefault(A, ConstructEmpty);

		operation::getDefault(ConstructEmpty, A)({ctv_a});

		exec::fillVtablePtr(ctv_a, A);


		auto member = exec::getVirtualMember(ctv_a, A, symbol_z);

		// @TODO: constructors should construct virtual parents
		// assert(member.getData<uint8_t>().front() == 7,
		//        base::strConcat("Constructor of A didn't set field in virtual parent ",
		//                         (u64)member.getData<uint8_t>().front()));
	}

	void vtable_creation_test() {
		/*
				A
				|v
				B
				|
				C
		*/


		ts::TypeDesc<> int_desc(ts::IntegralInfo::create(8));
		auto symbol0 = symtable::SymbolId::next();

		ts::ClassInfo A(ts::ClassInfo::create(base::StrId("A"), {{int_desc, symbol0}}));
		ts::TypeDesc<> A_desc(A);

		auto symbol1 = symtable::SymbolId::next();

		ts::ClassInfo B(
			ts::ClassInfo::create(base::StrId("B"),
		                          {{int_desc, symbol1}},
		                          {{A, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public)}},
		                          0));
		ts::TypeDesc<> B_desc(B);


		auto symbol2 = symtable::SymbolId::next();


		ts::ClassInfo C(ts::ClassInfo::create(
			base::StrId("C"),
			{{int_desc, symbol2}},
			{{B, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public)}},
			0));
		ts::TypeDesc<> C_desc(C);


		exec::CTV C_ctv = exec::alloc_new(C, C.getSize());


		namespace op = operation;

		op::createAddDefault(A, operation::Defaultable::ConstructEmpty);
		op::createAddDefault(B, operation::Defaultable::ConstructEmpty);
		op::createAddDefault(C, operation::Defaultable::ConstructEmpty);

		op::createAddDefault(A, operation::Defaultable::Equality);
		op::createAddDefault(B, operation::Defaultable::Equality);
		op::createAddDefault(C, operation::Defaultable::Equality);


		auto b_off = C.getAncestorInfo(B).start_offset.value();

		auto B_ctv = C_ctv.subCTV(B, b_off, B.getSize());

		exec::fillAllVtablePtrs(C_ctv);


		auto fun = op::createDefaultVirtualEquality(B);


		auto res = fun({B_ctv, B_ctv});


		assert(res.getData<bool>()[0] == true, "b_ctv should be equal to itself");
		

		auto B_ctv_2 = exec::alloc_new(B, B.getSize());

		exec::fillAllVtablePtrs(B_ctv_2);


		// This calculates the offset by hand (it works, because we know the exact hierarchy and instance).
		auto offset_a_in_b = B.getVirtualAncestorOffset(A) + A.getMemberInfo(symbol0).start_offset.value();

		B_ctv_2.subCTV(ts::IntegralInfo::create(8), offset_a_in_b, 8).getData<uint8_t>().front() = 30;


		auto member_ctv = getVirtualMember(B_ctv_2, B, symbol0);
		assert(member_ctv.getData<uint8_t>().front() == 30, "This value should be set to 30 already.");


		auto res2 = fun({B_ctv, B_ctv_2});

		assert(res2.getData<bool>()[0] == false, "Those ctvs should not be equal");


		auto res3 = fun({B_ctv_2, B_ctv_2});

		assert(res3.getData<bool>()[0] == true, "b_ctv_2 should be equal to itself");


		auto ctv_c = getVirtualMember(C_ctv, C, symbol0);
		ctv_c.getData<uint8_t>().front() = 30;

		auto res4 = fun({B_ctv, B_ctv_2});

		assert(res4.getData<bool>()[0] == true, "Those ctvs should be equal now");
	}

	void tuple_tests() {
		auto int8 = ts::IntegralInfo::create(8);
		auto tuple = ts::TupleInfo::create({int8, int8});


		auto ctv = exec::alloc_new(tuple);
		auto ctv2 = exec::alloc_new(tuple);


		using enum operation::Defaultable;
		for (auto kind: std::vector<operation::Defaultable>{
				 ConstructEmpty, ConstructFull, Assign, Equality, Compare}) {
			operation::createAddDefault(tuple, kind);
		}

		const auto& construct =
			operation::getDefault(operation::Defaultable::ConstructEmpty, tuple);
		construct({ctv});
		construct({ctv2});


		const auto& equal = operation::getDefault(operation::Defaultable::Equality, tuple);

		auto res_equal = equal({ctv, ctv});
		assert(res_equal.getData<bool>().front() == true, "tuple value is not equal to itself.");

		res_equal = equal({ctv, ctv2});
		assert(res_equal.getData<bool>().front() == true, "Two empty tuples should be equal.");

		auto int_42 = exec::alloc_new(int8);
		int_42.getData<uint8_t>().front() = 42;

		auto int_37 = exec::alloc_new(int8);
		int_37.getData<uint8_t>().front() = 37;


		const auto& construct_full = operation::getDefault(ConstructFull, tuple);

		assert(
			construct_full.signature.getParameterTypeList().size() == 3,
			"Construct full should take 3 arguments: value being constructed and values of fields");

		auto ctv3 = exec::alloc_new(tuple);
		auto ctv4 = exec::alloc_new(tuple);

		construct_full({ctv3, int_37, int_42});
		construct_full({ctv4, int_42, int_37});

		res_equal = equal({ctv3, ctv4});
		assert(res_equal.getData<bool>().front() == false, "(37, 42) shall not be equal (42, 37).");

		auto ctv5 = exec::alloc_new(tuple);
		construct_full({ctv5, int_37, int_42});

		res_equal = equal({ctv3, ctv5});
		assert(res_equal.getData<bool>().front() == true, "(37, 42) shall be equal (37, 42).");
	}

public:
	~SimpleExecTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/exec/tests/")
