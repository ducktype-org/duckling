#include <algorithm>
#include <hir/symtable/scope_symbol_id.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>
#include <base/string_id.hpp>

class SimpleTypeSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleTypeSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple TypeSystem Test") {
		TESTER_ADD_TEST(simple_class);
		TESTER_ADD_TEST(simple_pointer);
		TESTER_ADD_TEST(class_size);
		TESTER_ADD_TEST(class_inheritance);
		TESTER_ADD_TEST(inheritance_offset_calculating);
		TESTER_ADD_TEST(ancestor_finding_test);
		TESTER_ADD_TEST(C3_test);
		TESTER_ADD_TEST(vtable_test);
		TESTER_ADD_TEST(optionals_emptiness);
		TESTER_ADD_TEST(C3_test_with_normal_inheritance);
		TESTER_ADD_TEST(show);
		TESTER_ADD_TEST(iterators_test);
	}

private:
	void simple_class() {
		ts::ClassInfo id_1 = ts::ClassInfo::create(base::StrId("1"), {});
		ts::ClassInfo id_2 = ts::ClassInfo::create(base::StrId("2"), {});

		assert(id_1 != id_2, "classes should be different");

		ts::TypeDesc<> desc_1(id_1);
		ts::TypeDesc<> desc_2(id_2);

		assert(desc_1.getType() == id_1, "wrong TypeNamedId");

		auto          symbol0 = symtable::SymbolId::next();
		ts::ClassInfo id_3    = ts::ClassInfo::create(base::StrId("3"), { { desc_1, symbol0 } });

		auto off = id_3.getMemberInfo(symbol0);

		assert(
			off.start_offset == 0 && off.end_offset == 0, "offsets of empty class should be zero"
		);
	}

	void simple_pointer() {
		ts::TypeDesc<> desc_1(ts::ClassInfo::create(base::StrId("1"), {}));
		ts::TypeDesc<> desc_2(ts::ClassInfo::create(base::StrId("2"), {}));

		ts::PointerInfo ptr_1  = ts::PointerInfo::create(desc_1);
		ts::PointerInfo ptr_1_ = ts::PointerInfo::create(desc_1);
		ts::PointerInfo ptr_2  = ts::PointerInfo::create(desc_2);

		assert(ptr_1 != ptr_2, "Pointer to different types should be the different.");

		assert(ptr_1 == ptr_1_, "Pointer to the same type should be the same.");

		assert(ptr_1.getSize() == ts::POINTER_SIZE, "Wrong pointer size.");
	}

	void class_size() {
		ts::TypeDesc<> desc_1(ts::ClassInfo::create(base::StrId("1"), {}));

		ts::PointerInfo ptr_1 = ts::PointerInfo::create(desc_1);
		ts::PointerInfo ptr_2 = ts::PointerInfo::create(desc_1);

		ts::TypeDesc<> ptr_desc_1(ptr_1);
		ts::TypeDesc<> ptr_desc_2(ptr_2);

		auto           symbol0 = symtable::SymbolId::next();
		auto           symbol1 = symtable::SymbolId::next();
		ts::TypeDesc<> desc_3(ts::ClassInfo::create(
			base::StrId("3"), { { ptr_desc_1, symbol0 }, { ptr_desc_2, symbol1 } }
		));


		assert(desc_3.getType().getSize() == 2 * 64, "size of class incorrect");

		auto res = ts::ClassInfo(desc_3.getType()).getMemberInfo(symbol0);
		assert(
			res.start_offset == 0 && res.end_offset == 64,
			"the offsets of a pointer member are wrong"
		);
	}

	void class_inheritance() {
		ts::TypeDesc<> desc_0(ts::ClassInfo::create(base::StrId("0"), {}));
		assert(desc_0.getType().getSize() == 0, "Empty class isn't empty");

		auto symbol0             = symtable::SymbolId::next();
		auto multiple_use_symbol = symtable::SymbolId::next();

		ts::ClassInfo base_class = ts::ClassInfo::create(
			base::StrId("base"), { { desc_0, symbol0 }, { desc_0, multiple_use_symbol } }
		);

		assert(base_class.getSize() == 0, "A class with an empty class isn't empty");

		ts::ClassInfo inheriting_class = ts::ClassInfo::create(
			base::StrId("inheriting"),
			{ { desc_0, multiple_use_symbol } },
			{ { base_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		assert(
			inheriting_class.getSize() == 0, "A class inheriting from an empty class isn't empty"
		);

		auto res = inheriting_class.getMemberInfo(symbol0);
		assert(
			res.result_type == ts::ResultType::Standard,
			"Found wrong number of members or thought it's virtual"
		);
		assert(
			res.start_offset == 0, "Beginning offset of member of parent calculated incorrectly"
		);
		assert(res.end_offset == 0, "End offset of member of parent calculated incorrectly");

		ts::ClassInfo virtually_inheriting_class = ts::ClassInfo::create(
			base::StrId("virtually_inheriting"),
			{},
			{ { base_class, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		assert(
			virtually_inheriting_class.getSize() == ts::POINTER_SIZE,
			"A virtually inheriting class has a different size than just the vtable pointer"
		);

		res = virtually_inheriting_class.getMemberInfo(symbol0);
		assert(
			res.result_type == ts::ResultType::Virtual,
			"Found wrong number of members in virtual inheritance or thought the inheritance isn't "
			"virtual"
		);
		assert(res.last_virtual_ancestor == base_class, "Got the wrong virtual inheritance class");

		assert(
			res.start_offset == 0,
			"Beginning offset of member of parent calculated incorrectly in the virtual case"
		);
		assert(
			res.end_offset == 0,
			"End offset of member of parent calculated incorrectly in the virtual case"
		);

		auto not_used_symbol = symtable::SymbolId::next();

		res = inheriting_class.getMemberInfo(not_used_symbol);
		assert(
			res.result_type == ts::ResultType::NoResult, "Found symbol that wasn't in the class"
		);

		res = inheriting_class.getMemberInfo(multiple_use_symbol);
		assert(
			res.result_type == ts::ResultType::Ambiguous, "Wrongly counted occurrences of a symbol"
		);

		res = inheriting_class.getMemberInfo(multiple_use_symbol, { base_class });
		assert(res.result_type == ts::ResultType::Standard, "Hint didn't help");

		ts::ClassInfo double_inheriting_class = ts::ClassInfo::create(
			base::StrId("double_inheriting"),
			{},
			{ { inheriting_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);


		res = double_inheriting_class.getMemberInfo(multiple_use_symbol);
		assert(
			res.result_type == ts::ResultType::Ambiguous, "Didn't notice two occurrences of symbol"
		);
	}

	void optionals_emptiness() {
		ts::TypeDesc<> desc_0(ts::ClassInfo::create(base::StrId("0"), {}));

		auto symbol0             = symtable::SymbolId::next();
		auto multiple_use_symbol = symtable::SymbolId::next();

		ts::ClassInfo base_class = ts::ClassInfo::create(
			base::StrId("base"), { { desc_0, symbol0 }, { desc_0, multiple_use_symbol } }
		);

		ts::ClassInfo inheriting_class = ts::ClassInfo::create(
			base::StrId("inheriting"),
			{ { desc_0, multiple_use_symbol } },
			{ { base_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);

		auto not_used_symbol = symtable::SymbolId::next();

		auto res = inheriting_class.getMemberInfo(not_used_symbol);
		assert(
			res.result_type == ts::ResultType::NoResult, "Found symbol that wasn't in the class"
		);
		assert(
			!res.start_offset.has_value() && !res.end_offset.has_value() && !res.desc.has_value()
				&& !res.last_virtual_ancestor.has_value(),
			"There was no result but optional had a value"
		);

		res = inheriting_class.getMemberInfo(multiple_use_symbol);
		assert(
			res.result_type == ts::ResultType::Ambiguous, "Wrongly counted occurrences of a symbol"
		);
		assert(
			!res.start_offset.has_value() && !res.end_offset.has_value() && !res.desc.has_value()
				&& !res.last_virtual_ancestor.has_value(),
			"There was an ambiguous result but optional had a value"
		);

		res = inheriting_class.getMemberInfo(multiple_use_symbol, { base_class });
		assert(res.result_type == ts::ResultType::Standard, "Hint didn't help");
		assert(
			res.start_offset.has_value() && res.end_offset.has_value() && res.desc.has_value()
				&& !res.last_virtual_ancestor.has_value(),
			"Wrong optionals had a value for standard inheritance"
		);

		ts::ClassInfo double_inheriting_class = ts::ClassInfo::create(
			base::StrId("double_inheriting"),
			{},
			{ { inheriting_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);


		res = double_inheriting_class.getMemberInfo(multiple_use_symbol);
		assert(
			res.result_type == ts::ResultType::Ambiguous, "Didn't notice two occurrences of symbol"
		);
		assert(
			!res.start_offset.has_value() && !res.end_offset.has_value() && !res.desc.has_value()
				&& !res.last_virtual_ancestor.has_value(),
			"There was an ambiguous result but optional had a value"
		);
	}

	std::pair<usize, usize> get_member_offsets(
		ts::ClassInfo                inheriting_class,
		symtable::SymbolId           symbol,
		std::vector<usize>&          begin_vector,
		std::vector<usize>&          end_vector,
		std::vector<ts::ClassInfo>&& hint = {}
	) {
		auto r0 = inheriting_class.getMemberInfo(symbol, hint);

		assert(r0.isOk(), "Invalid result count");
		usize begin_offset0, end_offset0;
		if (r0.result_type == ts::ResultType::Virtual) {
			usize ancestor_offset
				= inheriting_class.getVirtualAncestorOffset(r0.last_virtual_ancestor.value());

			begin_offset0 = r0.start_offset.value() + ancestor_offset;
			end_offset0   = r0.end_offset.value() + ancestor_offset;


		} else {
			begin_offset0 = r0.start_offset.value();
			end_offset0   = r0.end_offset.value();
		}
		assert(
			end_offset0 == begin_offset0 + r0.desc.value().getType().getSize(),
			"Allocated wrong amount of space for the type"
		);
		begin_vector.push_back(begin_offset0);
		end_vector.push_back(end_offset0);
		return { begin_offset0, end_offset0 };
	}

	std::pair<usize, usize> get_member_offsets(
		ts::ClassInfo                inheriting_class,
		symtable::SymbolId           symbol,
		std::vector<ts::ClassInfo>&& hint = {}
	) {
		std::vector<usize> v0, v1;
		return get_member_offsets(inheriting_class, symbol, v0, v1, std::move(hint));
	}

	static std::pair<usize, usize> get_vtable_ptr_offset(
		ts::ClassInfo              inheriting_class,
		std::vector<ts::ClassInfo> class_with_virtual_inh,
		std::vector<usize>&        begin_offsets,
		std::vector<usize>&        end_offsets
	) {
		usize begin_offset;
		usize end_offset;
		if (inheriting_class == class_with_virtual_inh.back()) {
			begin_offset = inheriting_class.getVtablePtrOffset();
			end_offset   = begin_offset + ts::POINTER_SIZE;
		} else {
			auto result  = inheriting_class.getAncestorInfo(class_with_virtual_inh);
			begin_offset = result.end_offset.value() - ts::POINTER_SIZE;
			end_offset   = result.end_offset.value();
		}
		begin_offsets.push_back(begin_offset);
		end_offsets.push_back(end_offset);
		return { begin_offset, end_offset };
	}

	static std::pair<usize, usize> get_vtable_ptr_offset(
		ts::ClassInfo       inheriting_class,
		ts::ClassInfo       class_with_virtual_inh,
		std::vector<usize>& begin_offsets,
		std::vector<usize>& end_offsets
	) {
		return get_vtable_ptr_offset(
			inheriting_class,
			(std::vector<ts::ClassInfo>){ class_with_virtual_inh },
			begin_offsets,
			end_offsets
		);
	}

	static std::pair<usize, usize> get_vtable_ptr_offset(
		ts::ClassInfo inheriting_class, std::vector<ts::ClassInfo> class_with_virtual_inh
	) {
		std::vector<usize> v0, v1;
		return get_vtable_ptr_offset(inheriting_class, class_with_virtual_inh, v0, v1);
	}

	static std::pair<usize, usize> get_vtable_ptr_offset(
		ts::ClassInfo inheriting_class, ts::ClassInfo class_with_virtual_inh
	) {
		std::vector<usize> v0, v1;
		return get_vtable_ptr_offset(inheriting_class, class_with_virtual_inh, v0, v1);
	}

	void inheritance_offset_calculating() {
		ts::TypeDesc<> raw_ptr_desc(ts::RawPointerInfo::create());
		auto           symbol0 = symtable::SymbolId::next();
		auto           symbol1 = symtable::SymbolId::next();
		auto           symbol2 = symtable::SymbolId::next();

		ts::ClassInfo virtual_class
			= ts::ClassInfo::create(base::StrId("virtual"), { { raw_ptr_desc, symbol0 } });
		ts::ClassInfo parent_class = ts::ClassInfo::create(
			base::StrId("parent_class"),
			{ { raw_ptr_desc, symbol1 } },
			{ { virtual_class, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		assert(virtual_class.getSize() == ts::POINTER_SIZE, "Virtual class of wrong size");
		assert(parent_class.getSize() == 3 * ts::POINTER_SIZE, "Parent size of wrong size");

		ts::ClassInfo inheriting_class = ts::ClassInfo::create(
			base::StrId("ineriting"),
			{ { raw_ptr_desc, symbol2 } },
			{ { parent_class, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { virtual_class, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		assert(
			inheriting_class.getSize() == 5 * ts::POINTER_SIZE, "Inheriting class of wrong size"
		);

		std::vector<usize> begin_offsets;
		std::vector<usize> end_offsets;

		get_member_offsets(inheriting_class, symbol0, begin_offsets, end_offsets);
		get_member_offsets(inheriting_class, symbol1, begin_offsets, end_offsets);
		get_member_offsets(inheriting_class, symbol2, begin_offsets, end_offsets);

		get_vtable_ptr_offset(inheriting_class, inheriting_class, begin_offsets, end_offsets);
		get_vtable_ptr_offset(inheriting_class, parent_class, begin_offsets, end_offsets);

		std::sort(begin_offsets.begin(), begin_offsets.end());
		std::sort(end_offsets.begin(), end_offsets.end());

		for (usize i = 0; i < begin_offsets.size(); i++)
			std::cerr << begin_offsets[i] << " " << end_offsets[i] << std::endl;

		assert(begin_offsets[0] == 0, "First member's memory doesn't align with the class start");
		for (usize i = 1; i < begin_offsets.size(); i++) {
			assert(
				begin_offsets[i] >= end_offsets[i - 1],
				"There is overlap between members in class memory"
			);
			assert(
				begin_offsets[i] <= end_offsets[i - 1],
				"There is free space between members in class memory"
			);
		}
	}

	void ancestor_finding_test() {
		/*
		 *   F     F   F
		 *  /v    /   /v
		 * D   F E   D
		 *  \ /v  \ /
		 *   B     C
		 *    \   /
		 *      A
		 */

		ts::TypeDesc<> example_desc(ts::RawPointerInfo::create());
		auto           f = symtable::SymbolId::next();
		auto           e = symtable::SymbolId::next();
		auto           d = symtable::SymbolId::next();
		auto           c = symtable::SymbolId::next();
		auto           b = symtable::SymbolId::next();
		auto           a = symtable::SymbolId::next();

		ts::ClassInfo F = ts::ClassInfo::create(base::StrId("F"), { { example_desc, f } }, {}, 0);
		ts::ClassInfo E = ts::ClassInfo::create(
			base::StrId("E"),
			{ { example_desc, e } },
			{ { F, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo D = ts::ClassInfo::create(
			base::StrId("D"),
			{ { example_desc, d } },
			{ { F, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo C = ts::ClassInfo::create(
			base::StrId("C"),
			{ { example_desc, c } },
			{ { E, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { D, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo B = ts::ClassInfo::create(
			base::StrId("B"),
			{ { example_desc, b } },
			{ { D, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { F, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);

		ts::ClassInfo A = ts::ClassInfo::create(
			base::StrId("A"),
			{ { example_desc, a } },
			{ { B, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { C, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);

		std::vector<usize> begin_offsets;
		std::vector<usize> end_offsets;

		get_member_offsets(C, e);

		get_member_offsets(A, a, begin_offsets, end_offsets);
		get_member_offsets(A, b, begin_offsets, end_offsets);
		get_member_offsets(A, c, begin_offsets, end_offsets);
		get_member_offsets(A, d, begin_offsets, end_offsets, { C });
		get_member_offsets(A, d, begin_offsets, end_offsets, { B });
		get_member_offsets(A, e, begin_offsets, end_offsets);
		get_member_offsets(A, f, begin_offsets, end_offsets, { C, D });
		get_member_offsets(A, f, begin_offsets, end_offsets, { C, E });

		get_vtable_ptr_offset(A, A, begin_offsets, end_offsets);
		get_vtable_ptr_offset(A, B, begin_offsets, end_offsets);
		get_vtable_ptr_offset(A, C, begin_offsets, end_offsets);
		get_vtable_ptr_offset(A, { C, D }, begin_offsets, end_offsets);
		get_vtable_ptr_offset(A, { B, D }, begin_offsets, end_offsets);

		for (usize i = 0; i < begin_offsets.size(); i++)
			std::cout << begin_offsets[i] << " " << end_offsets[i] << std::endl;

		std::sort(begin_offsets.begin(), begin_offsets.end());
		std::sort(end_offsets.begin(), end_offsets.end());

		for (usize i = 0; i < begin_offsets.size(); i++)
			std::cout << begin_offsets[i] << " " << end_offsets[i] << std::endl;

		assert(begin_offsets[0] == 0, "First member's memory doesn't align with the class start");
		for (usize i = 1; i < begin_offsets.size(); i++) {
			assert(
				begin_offsets[i] >= end_offsets[i - 1],
				"There is overlap between members in class memory"
			);
			assert(
				begin_offsets[i] <= end_offsets[i - 1],
				"There is free space between members in class memory"
			);
		}

		assert(
			get_member_offsets(A, f, { B, D }) == get_member_offsets(A, f, { C, D }),
			"Two instances of the virtual went to different places"
		);
	}

	void C3_test() {
		/*
		 *     D
		 *   v/ \v
		 *   B   C
		 *   v\ /v
		 *     A
		 */
		ts::TypeDesc<> example_desc(ts::RawPointerInfo::create());
		auto           d = symtable::SymbolId::next();
		auto           c = symtable::SymbolId::next();
		auto           b = symtable::SymbolId::next();
		auto           a = symtable::SymbolId::next();

		ts::ClassInfo D = ts::ClassInfo::create(base::StrId("D"), { { example_desc, d } }, {}, 0);
		ts::ClassInfo C = ts::ClassInfo::create(
			base::StrId("C"),
			{ { example_desc, c } },
			{ { D, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo B = ts::ClassInfo::create(
			base::StrId("B"),
			{ { example_desc, b } },
			{ { D, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo A = ts::ClassInfo::create(
			base::StrId("A"),
			{ { example_desc, a } },
			{ { B, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) },
		      { C, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);

		assert(
			A.getVirtualAncestorOffset(D) > A.getVirtualAncestorOffset(B),
			"D wasn't after B in our memory"
		);
		assert(
			A.getVirtualAncestorOffset(D) > A.getVirtualAncestorOffset(C),
			"D wasn't after C in our memory"
		);
		assert(
			A.getVirtualAncestorOffset(C) > A.getVirtualAncestorOffset(B),
			"C wasn't after B in our memory"
		);
	}

	usize getInVirtualAncestorOffset(ts::ClassInfo child, ts::ClassInfo ancestor) {
		auto result      = child.getAncestorInfo(ancestor);
		auto in_ancestor = result.last_virtual_ancestor.value();
		return child.getVirtualAncestorOffset(in_ancestor) + result.start_offset.value();
	}

	void C3_test_with_normal_inheritance() {
		/*               G
		 *             v/
		 *  E    D    F
		 *   \ v/ \v /
		 *    B    C
		 *    v\  /v
		 *      A
		 */
		ts::TypeDesc<> example_desc(ts::RawPointerInfo::create());
		auto           g = symtable::SymbolId::next();
		auto           f = symtable::SymbolId::next();
		auto           e = symtable::SymbolId::next();
		auto           d = symtable::SymbolId::next();
		auto           c = symtable::SymbolId::next();
		auto           b = symtable::SymbolId::next();
		auto           a = symtable::SymbolId::next();

		ts::ClassInfo G = ts::ClassInfo::create(base::StrId("G"), { { example_desc, g } }, {}, 0);
		ts::ClassInfo F = ts::ClassInfo::create(
			base::StrId("F"),
			{ { example_desc, f } },
			{ { G, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo E = ts::ClassInfo::create(base::StrId("E"), { { example_desc, e } }, {}, 0);
		ts::ClassInfo D = ts::ClassInfo::create(base::StrId("D"), { { example_desc, d } }, {}, 0);
		ts::ClassInfo C = ts::ClassInfo::create(
			base::StrId("C"),
			{ { example_desc, c } },
			{ { D, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) },
		      { F, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo B = ts::ClassInfo::create(
			base::StrId("B"),
			{ { example_desc, b } },
			{ { D, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) },
		      { E, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo A = ts::ClassInfo::create(
			base::StrId("A"),
			{ { example_desc, a } },
			{ { B, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) },
		      { C, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);

		assert(
			A.getVirtualAncestorOffset(D) > A.getVirtualAncestorOffset(B),
			"D wasn't after B in our memory"
		);
		assert(
			A.getVirtualAncestorOffset(D) > A.getVirtualAncestorOffset(C),
			"D wasn't after C in our memory"
		);
		assert(
			A.getVirtualAncestorOffset(C) > A.getVirtualAncestorOffset(B),
			"C wasn't after B in our memory"
		);
		assert(
			getInVirtualAncestorOffset(A, F) == A.getVirtualAncestorOffset(C),
			"F wasn't at C in our memory"
		);
		assert(
			getInVirtualAncestorOffset(A, E) == A.getVirtualAncestorOffset(B),
			"E wasn't at B in our memory"
		);
		assert(
			A.getVirtualAncestorOffset(G) > getInVirtualAncestorOffset(A, F),
			"G wasn't after F in our memory"
		);
	}

	void vtable_test() {
		ts::TypeDesc<> example_desc(ts::RawPointerInfo::create());
		ts::ClassInfo  X          = ts::ClassInfo::create(base::StrId("X"), {}, {}, 0);
		ts::ClassInfo  doubleVirt = ts::ClassInfo::create(
            base::StrId("doubleVirt"),
            {},
            { { X, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
            1
        );
		ts::ClassInfo virtInh = ts::ClassInfo::create(
			base::StrId("virtInh"),
			{},
			{ { X, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo virtMethod  = ts::ClassInfo::create(base::StrId("virtMethod"), {}, {}, 1);
		ts::ClassInfo nothingVirt = ts::ClassInfo::create(base::StrId("nothingVirt"), {}, {}, 0);

		assert(
			nothingVirt.getVtableSize() == 0, "Class with nothing virtual has positive size vtable"
		);
		assert(virtInh.getVtableSize() == 1, "Class with virt inheritance has wrong size vtable");
		assert(virtMethod.getVtableSize() == 1, "Class with virt method has wrong size vtable");
		assert(
			doubleVirt.getVtableSize() == 2,
			"Class with virtual method and parent has wrong size vtable"
		);

		try {
			nothingVirt.getVtablePtrOffset();
			fail("A class with no vtable returned a vtable pointer");
		} catch (std::exception& e) {}
		virtInh.getVtablePtrOffset();
		virtMethod.getVtablePtrOffset();
		doubleVirt.getVtablePtrOffset();
	}

	void assert_sorted_members(ts::ClassInfo A, const std::vector<ts::MemberData>& members) {
		base::Optional<ts::ClassInfo> last_virtual_ancestor;
		ssize_t                      last_offset = -1;
		for (auto member_data: members) {
			if (!last_virtual_ancestor.has_value()) {
				if (!member_data.last_virtual_ancestor.has_value()) {
					assert(
						last_offset < (ssize_t) member_data.offset,
						"Members weren't sorted (case 1)"
					);
				}
			} else if (member_data.last_virtual_ancestor.has_value()) {
				if (member_data.last_virtual_ancestor.value() == last_virtual_ancestor.value()) {
					assert(member_data.offset > last_offset, "Members weren't sorted (case 2)");
				} else {
					auto last_va = last_virtual_ancestor.value();
					auto this_va = member_data.last_virtual_ancestor.value();
					assert(
						A.getVirtualAncestorOffset(last_va) < A.getVirtualAncestorOffset(this_va),
						"Members weren't sorted (case 3)"
					);
				}
			} else {
				fail("Members weren't sorted (case 4)");
			}
			last_offset           = (ssize_t) member_data.offset;
			last_virtual_ancestor = member_data.last_virtual_ancestor;
		}
	}

	void assert_sorted_ancestors(ts::ClassInfo A, const std::vector<ts::AncestorData>& ancestors) {
		base::Optional<ts::ClassInfo> last_virtual_ancestor;
		ssize_t                      last_offset = -1;
		usize                        last_size   = 0;
		for (auto ancestor_data: ancestors) {
			if (!last_virtual_ancestor.has_value()) {
				if (!ancestor_data.last_virtual_ancestor.has_value()) {
					if (last_offset == ancestor_data.offset) {
						assert(
							last_size > ancestor_data.info.getSize(),
							"Ancestors weren't sorted (case 1)"
						);
					} else {
						assert(
							last_offset < (ssize_t) ancestor_data.offset,
							"Ancestors weren't sorted (case 2)"
						);
					}
				}
			} else if (ancestor_data.last_virtual_ancestor.has_value()) {
				if (ancestor_data.last_virtual_ancestor.value() == last_virtual_ancestor.value()) {
					if (last_offset == ancestor_data.offset) {
						assert(
							last_size > ancestor_data.info.getSize(),
							"Ancestors weren't sorted (case 3)"
						);
					} else {
						assert(
							ancestor_data.offset >= last_offset, "Ancestors weren't sorted (case 4)"
						);
					}
				} else {
					auto last_va = last_virtual_ancestor.value();
					auto this_va = ancestor_data.last_virtual_ancestor.value();
					assert(
						A.getVirtualAncestorOffset(last_va) < A.getVirtualAncestorOffset(this_va),
						"Ancestors weren't sorted (case 5)"
					);
				}
			} else {
				fail("Ancestors weren't sorted (case 6)");
			}
			last_offset           = (ssize_t) ancestor_data.offset;
			last_virtual_ancestor = ancestor_data.last_virtual_ancestor;
			last_size             = ancestor_data.info.getSize();
		}
	}

	void iterators_test() {
		/*
		 *   F     F   F
		 *  /v    /   /v
		 * D   F E   D
		 *  \ /v  \ /
		 *   B     C
		 *    \   /
		 *      A
		 */

		ts::TypeDesc<> example_desc(ts::RawPointerInfo::create());
		auto           f = symtable::SymbolId::next();
		auto           e = symtable::SymbolId::next();
		auto           d = symtable::SymbolId::next();
		auto           c = symtable::SymbolId::next();
		auto           b = symtable::SymbolId::next();
		auto           a = symtable::SymbolId::next();

		ts::ClassInfo F = ts::ClassInfo::create(base::StrId("F"), { { example_desc, f } }, {}, 0);
		ts::ClassInfo E = ts::ClassInfo::create(
			base::StrId("E"),
			{ { example_desc, e } },
			{ { F, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo D = ts::ClassInfo::create(
			base::StrId("D"),
			{ { example_desc, d } },
			{ { F, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo C = ts::ClassInfo::create(
			base::StrId("C"),
			{ { example_desc, c } },
			{ { E, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { D, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);
		ts::ClassInfo B = ts::ClassInfo::create(
			base::StrId("B"),
			{ { example_desc, b } },
			{ { D, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { F, ts::InheritanceTag(true, ts::InheritanceTag::Kind::Public) } },
			0
		);

		ts::ClassInfo A = ts::ClassInfo::create(
			base::StrId("A"),
			{ { example_desc, a } },
			{ { B, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) },
		      { C, ts::InheritanceTag(false, ts::InheritanceTag::Kind::Public) } },
			0
		);


		assert(F.members().size() == 1, "F had a wrong number of members");
		assert(E.members().size() == 1, "E had a wrong number of members");
		assert(D.members().size() == 1, "D had a wrong number of members");
		assert(C.members().size() == 1, "C had a wrong number of members");
		assert(B.members().size() == 1, "B had a wrong number of members");
		assert(A.members().size() == 1, "A had a wrong number of members");

		assert(F.basicParents().size() == 0, "F had a wrong number of normal parents");
		assert(E.basicParents().size() == 1, "E had a wrong number of normal parents");
		assert(D.basicParents().size() == 0, "D had a wrong number of normal parents");
		assert(C.basicParents().size() == 2, "C had a wrong number of normal parents");
		assert(B.basicParents().size() == 1, "B had a wrong number of normal parents");
		assert(A.basicParents().size() == 2, "A had a wrong number of normal parents");

		assert(F.virtualAncestors().size() == 0, "F had a wrong number of virtual ancestors");
		assert(E.virtualAncestors().size() == 0, "E had a wrong number of virtual ancestors");
		assert(D.virtualAncestors().size() == 1, "D had a wrong number of virtual ancestors");
		assert(C.virtualAncestors().size() == 1, "C had a wrong number of virtual ancestors");
		assert(B.virtualAncestors().size() == 1, "B had a wrong number of virtual ancestors");
		assert(A.virtualAncestors().size() == 1, "A had a wrong number of virtual ancestors");

		assert(F.allAncestors().size() == 0, "F had a wrong number of ancestors");
		assert(E.allAncestors().size() == 1, "E had a wrong number of ancestors");
		assert(D.allAncestors().size() == 1, "D had a wrong number of ancestors");
		assert(C.allAncestors().size() == 4, "C had a wrong number of ancestors");
		assert(B.allAncestors().size() == 2, "B had a wrong number of ancestors");
		assert(A.allAncestors().size() == 7, "A had a wrong number of ancestors");

		assert(
			F.allMembers().size() == F.allAncestors().size() + 1,
			"F had a wrong number of members (total)"
		);
		assert(
			E.allMembers().size() == E.allAncestors().size() + 1,
			"E had a wrong number of members (total)"
		);
		assert(
			D.allMembers().size() == D.allAncestors().size() + 1,
			"D had a wrong number of members (total)"
		);
		assert(
			C.allMembers().size() == C.allAncestors().size() + 1,
			"C had a wrong number of members (total)"
		);
		assert(
			B.allMembers().size() == B.allAncestors().size() + 1,
			"B had a wrong number of members (total)"
		);
		assert(
			A.allMembers().size() == A.allAncestors().size() + 1,
			"A had a wrong number of members (total)"
		);


		assert_sorted_members(A, A.allMembers());
		assert_sorted_members(A, A.members());

		assert_sorted_ancestors(A, A.allAncestors());
		assert_sorted_ancestors(A, A.basicParents());
	}

	void show() {
		auto int8 = ts::IntegralInfo::create(8);
		assert(int8.show() == "int_8", "Incorrect int8 representation: " + int8.show());

		auto pointer = ts::PointerInfo::create(int8);
		assert(
			pointer.show() == "pointer(int_8)",
			"Incorrect pointer representation: " + pointer.show()
		);

		auto fun = ts::FunctionInfo::create({ pointer, int8 }, int8);
		assert(
			fun.show() == "Function (pointer(int_8),int_8,) -> (int_8)",
			"Incorrect function representation: " + fun.show()
		);

		auto class_t = ts::ClassInfo::create(base::StrId("A"), {});
		assert(class_t.show() == "Class: A", "Incorrect class representation: " + class_t.show());
	}

public:
	~SimpleTypeSystemTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/typesystem/tests/")
