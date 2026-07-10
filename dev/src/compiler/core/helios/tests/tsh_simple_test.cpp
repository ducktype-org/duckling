#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <any>

using namespace compiler::tsh;

/**
 * This test class contains tests checking the most basic and boring functionality of the
 typesystem.

 * In particular, types of each `Kind` should be checked for correct dynamic casting of pImpl.
 * <br>
 * *Note, that this should be checked for each kind, because implementation of that functionality
 * consists of a few macros, which must be ultimately must be added manually. Some more complex
 * kinds of types, like classes, may have their definitions in a different place than most other
 * kinds, so checking correctness of implementation cannot be reduced to visual checks.*

 * Other than that, type sizes and contents after construction are checked.

 * Additionally, if a kind of types is supposed to unify types under some condition
 * (e.g. tuples are structurally unified), this should be checked.

 * The converse (i.e. that two types are not unified when they shouldn't be) should also be checked.
 */
class HigherTypeSystemSimpleTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HigherTypeSystemSimpleTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(trivialCastAndAssignment);
		TESTER_ADD_TEST(simpleVoidAndUnit);
		TESTER_ADD_TEST(simpleByteSized);
		TESTER_ADD_TEST(simpleInts);
		TESTER_ADD_TEST(simpleFloats);
		TESTER_ADD_TEST(simplePointer);
		TESTER_ADD_TEST(simpleString);
		TESTER_ADD_TEST(simpleDynamicArray);
		TESTER_ADD_TEST(simpleStaticArray);
		TESTER_ADD_TEST(simpleTuple);
		TESTER_ADD_TEST(simpleVariant);
		TESTER_ADD_TEST(simpleFunction);
		TESTER_ADD_TEST(simpleLanguageElements);
		TESTER_ADD_TEST(simpleMeta);
		TESTER_ADD_TEST(simpleTypeTemplate);
		TESTER_ADD_TEST(simpleExpressionType);
		TESTER_ADD_TEST(simpleValueCategory);
		TESTER_ADD_TEST(simpleImplicitCoercibility);
	}

private:
	using enum Kind;

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getIntegralTypeNoContext(
		u64 size, compiler::tsh::IntegralAbstractType::Signedness signedness
	) {
		return std::any_cast<compiler::tsh::IntegralAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return compiler::tsh::getIntegralType(ctx, size, signedness);
			})
		);
	}

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getFloatTypeNoContext(u64 size) {
		return std::any_cast<compiler::tsh::FloatAbstractType>(query::utils::withContextCompute(
			[&](query::Context& ctx) { return compiler::tsh::getFloatType(ctx, size); }
		));
	}

	/**
	 * Test that the specialized AbstractType to AbstractType dynamic cast works as intended.
	 * Also, test that assignment works.
	 */
	void trivialCastAndAssignment() {
		const AbstractType type_1{ getVoidType() };
		AbstractType       type_2 = type_1;
		assertTrue(type_1 == type_2, "The trivial dynamic cast should not change any objects.");
		type_2 = getUnitType();
		assertTrue(type_1 != type_2, "Assignment on AbstractType should change the target object.");
	}

	/**
	 * Test that there is only one void and one unit type, and that they are correctly cast.
	 */
	void simpleVoidAndUnit() {
		const auto void_1 = getVoidType();
		const auto void_2 = getVoidType();
		assertTrue(void_1 == void_2, "There should only be one Void type.");
		assertTrue(void_1.getKind() == Void, "Void type should have kind Void.");

		const auto unit_1 = getUnitType();
		const auto unit_2 = getUnitType();
		assertTrue(unit_1 == unit_2, "There should only be one Unit type.");
		assertTrue(unit_1.getKind() == Unit, "Unit type should have kind Unit.");

		assertTrue(void_1 != unit_1, "Void and Unit should be different types.");

		const AbstractType     type_void = void_1;
		const VoidAbstractType void_3    = type_void;
		assertTrue(void_3.getKind() == Void, "Void should survive casting.");

		const AbstractType     type_unit = unit_1;
		const UnitAbstractType unit_3    = type_unit;
		assertTrue(unit_3.getKind() == Unit, "Unit should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(void_1.hasNoOpDestructor(ctx), "Void should have no op destructor.");
			assertTrue(unit_1.hasNoOpDestructor(ctx), "Unit should have no op destructor.");

			const auto unit_st = st(unit_1);
			assertTrue(unit_st.isDefaultConstructible(ctx), "Unit should be default constructible.");
			assertFalse(
				unit_st.isTriviallyZeroInitializable(ctx),
				"Unit shouldn't be trivially zero-initializable."
			);
			assertTrue(unit_st.isCopyable(ctx), "Unit should be copyable.");
			assertTrue(unit_st.isTriviallyCopyable(ctx), "Unit should be trivially copyable.");

			const auto void_st = st(void_1);
			assertFalse(
				void_st.isDefaultConstructible(ctx), "Void should be default constructible."
			);
			assertFalse(
				void_st.isTriviallyZeroInitializable(ctx),
				"Void should be trivially zero-initializable."
			);
			assertFalse(void_st.isCopyable(ctx), "Void should be copyable.");
			assertFalse(void_st.isTriviallyCopyable(ctx), "void should be trivially copyable.");
		});
	}

	/**
	 * Test that there are three unique byte-sized types, and that they are correctly cast.
	 */
	void simpleByteSized() {
		const auto byte_1 = getByteType();

		assertTrue(byte_1.getKind() == Byte, "Byte type should have kind Byte.");

		const auto bool_1 = getBoolType();

		assertTrue(bool_1.getKind() == Bool, "Bool type should have kind Bool.");

		const auto char_1 = getCharType();

		assertTrue(char_1.getKind() == Char, "Char type should have kind Char.");

		assertTrue(
			byte_1 != bool_1 && bool_1 != char_1 && char_1 != byte_1,
			"All byte-sized types should be different."
		);

		const auto byte_2 = getByteType();
		const auto bool_2 = getBoolType();
		const auto char_2 = getCharType();

		assertTrue(
			byte_1 == byte_2 && bool_1 == bool_2 && char_1 == char_2,
			"Byte-sized types of the same kind should be equal."
		);

		const AbstractType     type_byte = byte_1;
		const ByteAbstractType byte_3    = type_byte;
		assertTrue(byte_3.getKind() == Byte, "Byte should survive casting.");

		const AbstractType     type_bool = bool_1;
		const BoolAbstractType bool_3    = type_bool;
		assertTrue(bool_3.getKind() == Bool, "Bool should survive casting.");

		const AbstractType     type_char = char_1;
		const CharAbstractType char_3    = type_char;
		assertTrue(char_3.getKind() == Char, "Byte should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto bool_st = st(bool_1);
			assertTrue(bool_st.isDefaultConstructible(ctx), "Bool should be default constructible.");
			assertTrue(
				bool_st.isTriviallyZeroInitializable(ctx),
				"Bool should be trivially zero-initializable."
			);
			assertTrue(bool_st.isCopyable(ctx), "Bool should be copyable.");
			assertTrue(bool_st.isTriviallyCopyable(ctx), "Bool should be trivially copyable.");

			const auto byte_st = st(byte_1);
			assertTrue(byte_st.isDefaultConstructible(ctx), "Byte should be default constructible.");
			assertTrue(
				byte_st.isTriviallyZeroInitializable(ctx),
				"Byte should be trivially zero-initializable."
			);
			assertTrue(byte_st.isCopyable(ctx), "Byte should be copyable.");
			assertTrue(byte_st.isTriviallyCopyable(ctx), "Byte should be trivially copyable.");

			const auto char_st = st(char_1);
			assertTrue(char_st.isDefaultConstructible(ctx), "Char should be default constructible.");
			assertTrue(
				char_st.isTriviallyZeroInitializable(ctx),
				"Char should be trivially zero-initializable."
			);
			assertTrue(char_st.isCopyable(ctx), "Char should be copyable.");
			assertTrue(char_st.isTriviallyCopyable(ctx), "Char should be trivially copyable.");
		});
	}

	/**
	 * Test that there are signed and unsigned versions of each integral type
	 * of sizes from 8 bits to 128 bits, and that they are correctly cast.
	 */
	void simpleInts() {
		using enum IntegralAbstractType::Signedness;

		for (usize i = 0; i < 5; i++) {
			const auto int_1 = getIntegralTypeNoContext(8U * (1 << i), Signed);
			const auto int_2 = getIntegralTypeNoContext(8U * (1 << i), Signed);
			const auto int_u = getIntegralTypeNoContext(8U * (1 << i), Unsigned);

			assertTrue(int_1.getKind() == Integral, "Int should have kind Integral.");

			assertTrue(
				int_1.getSize() == Bits(8) * (1 << i), "Size of Int should be as constructed."
			);
			assertTrue(int_1 == int_2, "Ints of the same size and signedness should be the same.");
			assertTrue(
				int_1 != int_u,
				"Ints of the same size but different signedness should be the different."
			);

			AbstractType         type_int = int_1;
			IntegralAbstractType int_3    = type_int;
			assertTrue(int_3.getKind() == Integral, "Int should survive casting.");

			ASSERT_TRUE(int_1.getSignedness() == Signed);
			ASSERT_TRUE(int_2.getSignedness() == Signed);
			ASSERT_TRUE(int_u.getSignedness() == Unsigned);
			ASSERT_TRUE(int_3.getSignedness() == Signed);

			assertTrue(
				int_1.hasNoOpDestructor() && int_u.hasNoOpDestructor(),
				"Ints should have no op destructor."
			);
		}

		assertTrue(
			getIntegralTypeNoContext(8, Signed) != getIntegralTypeNoContext(16, Signed),
			"Ints of different sizes should be different."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto int_st = st(getIntegralType(ctx, 32, Signed));
			assertTrue(int_st.isDefaultConstructible(ctx), "Int should be default constructible.");
			assertTrue(
				int_st.isTriviallyZeroInitializable(ctx),
				"Int should be trivially zero-initializable."
			);
			assertTrue(int_st.isCopyable(ctx), "Int should be copyable.");
			assertTrue(int_st.isTriviallyCopyable(ctx), "Int should be trivially copyable.");

			const auto int_ref_st = int_st.withReferenceKind(ReferenceKind::Ref);
			assertFalse(
				int_ref_st.isDefaultConstructible(ctx),
				"Reference to Int should NOT be default constructible."
			);
			assertFalse(
				int_ref_st.isTriviallyZeroInitializable(ctx),
				"Reference to Int should NOT be trivially zero-initializable."
			);
			assertTrue(int_ref_st.isCopyable(ctx), "Reference to Int should be copyable.");
			assertTrue(
				int_ref_st.isTriviallyCopyable(ctx), "Reference to Int should be trivially copyable."
			);

			const auto int_box_st = int_st.withReferenceKind(ReferenceKind::Box);
			assertFalse(
				int_box_st.isDefaultConstructible(ctx),
				"Box of Int should NOT be default constructible."
			);
			assertFalse(
				int_box_st.isTriviallyZeroInitializable(ctx),
				"Box of Int should NOT be trivially zero-initializable."
			);
			assertTrue(int_box_st.isCopyable(ctx), "Box of Int should be copyable.");
			assertFalse(
				int_box_st.isTriviallyCopyable(ctx), "Box of Int should not be trivially copyable."
			);
		});
	}

	/**
	 * Test that there are floating point types of sizes from 16 bits to 128 bits,
	 * and that they are correctly cast.
	 */
	void simpleFloats() {
		for (const std::array<usize, 5> float_sizes = { 16, 32, 64, 80, 128 };
		     const usize                float_size: float_sizes) {
			auto float_1 = getFloatTypeNoContext(float_size);
			auto float_2 = getFloatTypeNoContext(float_size);

			assertTrue(
				float_1.getSize() == Bits(float_size), "Size of Float should be as constructed."
			);
			assertTrue(float_1 == float_2, "Floats of the same size should be the same.");
			assertTrue(float_1.getKind() == Float, "Floats should have float kind.");

			AbstractType      type_float = float_1;
			FloatAbstractType float_3    = type_float;
			assertTrue(float_3.getKind() == Float, "Float should survive casting.");

			assertTrue(float_1.hasNoOpDestructor(), "Floats should have no op destructor.");
		}

		assertTrue(
			getFloatTypeNoContext(32) != getFloatTypeNoContext(64),
			"Floats of different sizes should be different."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto float_st = st(getFloatType(ctx, 64));
			assertTrue(
				float_st.isDefaultConstructible(ctx), "Float should be default constructible."
			);
			assertTrue(
				float_st.isTriviallyZeroInitializable(ctx),
				"Float should be trivially zero-initializable."
			);
			assertTrue(float_st.isCopyable(ctx), "Float should be copyable.");
			assertTrue(float_st.isTriviallyCopyable(ctx), "Float should be trivially copyable.");
		});
	}

	using enum Mutability;

	static SymbolType<> st(const AbstractType abstract_type, const bool is_mutable = false) {
		return SymbolType{ abstract_type, ReferenceKind::Direct, is_mutable ? Mutable : Immutable };
	}

	/**
	 * Test that Raw Pointer and Pointer types correctly cast
	 * between each other and retain AbstractTypermation as expected.
	 */
	void simplePointer() {
		const auto raw_1 = getRawPointerType(false);
		assertTrue(raw_1.getKind() == RawPointer, "Raw Pointer should have kind RawPointer.");
		const auto raw_2 = getRawPointerType(true);
		assertTrue(
			raw_2.getKind() == RawPointer, "Mutable Raw Pointer should have kind RawPointer."
		);
		assertTrue(raw_1 != raw_2, "Immutable and mutable Raw Pointers should be different.");
		const auto raw_3 = getRawPointerType(false);
		assertTrue(raw_1 == raw_3, "There should be only one immutable Raw Pointer.");
		const auto raw_4 = getRawPointerType(true);
		assertTrue(raw_2 == raw_4, "There should be only one mutable Raw Pointer.");

		const AbstractType           type_raw = raw_1;
		const RawPointerAbstractType raw_5    = type_raw;
		assertTrue(raw_5.getKind() == RawPointer, "Raw Pointer should survive casting.");

		const auto ptr_1 = query::entryPoint<QueryPointerType>({ st(raw_1) });
		assertTrue(ptr_1.getKind() == Pointer, "Pointer should have kind Pointer.");
		const auto ptr_2 = query::entryPoint<QueryPointerType>(st(raw_1));
		assertTrue(ptr_1 == ptr_2, "Pointers to the same type and mutability should be equal.");
		const auto ptr_3 = query::entryPoint<QueryPointerType>(st(raw_2));
		assertTrue(ptr_1 != ptr_3, "Pointers to different types should be different.");
		const auto ptr_4 = query::entryPoint<QueryPointerType>(st(raw_1, true));
		assertTrue(ptr_1 != ptr_4, "Pointers of different mutability should be different.");

		const AbstractType        type_ptr = ptr_4;
		const PointerAbstractType ptr_5    = type_ptr;
		assertTrue(
			ptr_5.getKind() == Pointer && ptr_5.getPointee() == ptr_4.getPointee(),
			"Pointer should survive casting."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto raw_st = st(raw_1);
			assertTrue(
				raw_st.isDefaultConstructible(ctx), "RawPointer should be default constructible."
			);
			assertTrue(
				raw_st.isTriviallyZeroInitializable(ctx), "RawPointer should be zero initializable."
			);
			assertTrue(raw_st.isCopyable(ctx), "RawPointer should be copyable.");
			assertTrue(raw_st.isTriviallyCopyable(ctx), "RawPointer should be trivially copyable.");

			const auto pointer_st = st(ptr_1);
			assertTrue(
				pointer_st.isDefaultConstructible(ctx), "Pointer should be default constructible."
			);
			assertTrue(
				pointer_st.isTriviallyZeroInitializable(ctx),
				"Pointer should be trivially zero-initializable."
			);
			assertTrue(pointer_st.isCopyable(ctx), "Pointer should be copyable.");
			assertTrue(pointer_st.isTriviallyCopyable(ctx), "Pointer should be trivially copyable.");
		});
	}

	/**
	 * Test that there is only one string type, and that it is correctly cast.
	 */
	void simpleString() {
		const auto str_1 = getStringType();
		const auto str_2 = getStringType();

		assertTrue(str_1 == str_2, "There should only be one String type.");

		assertTrue(str_1.getKind() == String, "String type should have kind String.");

		const AbstractType       type_str = str_1;
		const StringAbstractType str_3    = type_str;
		assertTrue(str_3.getKind() == String, "String should survive casting.");

		assertFalse(str_1.hasNoOpDestructor(), "String should not have no op destructor.");

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto string_st = st(str_1);
			assertTrue(
				string_st.isDefaultConstructible(ctx), "String should be default constructible."
			);
			assertTrue(
				string_st.isTriviallyZeroInitializable(ctx),
				"String should be trivially zero-initializable."
			);
			assertTrue(string_st.isCopyable(ctx), "String should be copyable.");
			// @TODO: #2000 Make this check come back after unmocking copy constructors.
			// assertFalse(
			// 	string_st.isTriviallyCopyable(ctx), "String should not be trivially copyable."
			// );
		});
	}

	/**
	 * Test that dynamic array with different elements are different types
	 * and that they are correctly cast.
	 */
	void simpleDynamicArray() {
		using enum IntegralAbstractType::Signedness;

		const auto int_16 = getIntegralTypeNoContext(16, Signed);
		const auto int_32 = getIntegralTypeNoContext(32, Signed);

		const auto arr_1 = query::entryPoint<QueryDynamicArrayType>(st(int_16));
		assertTrue(arr_1.getKind() == DynamicArray, "DynamicArray should have kind DynamicArray.");
		assertTrue(arr_1.getElementType() == st(int_16), "Element type should be as constructed.");

		const AbstractType             type_arr = arr_1;
		const DynamicArrayAbstractType arr_2    = type_arr;
		assertTrue(arr_2.getKind() == DynamicArray, "DynamicArray should survive casting.");

		const auto arr_3 = query::entryPoint<QueryDynamicArrayType>(st(int_16));
		assertTrue(arr_1 == arr_3, "DynamicArrays with the same element types should be equal.");

		const auto arr_4 = query::entryPoint<QueryDynamicArrayType>(st(int_32));
		assertTrue(
			arr_1 != arr_4, "DynamicArrays with different element types should be different."
		);

		const auto arr_5 = query::entryPoint<QueryDynamicArrayType>(st(int_16, true));
		assertTrue(
			arr_1 != arr_5,
			"DynamicArrays with element with different mutability should be different."
		);

		assertFalse(
			arr_1.hasNoOpDestructor() && arr_4.hasNoOpDestructor(),
			"DynamicArrays should not have no op destructors."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto arr_st = st(arr_1);
			assertTrue(
				arr_st.isDefaultConstructible(ctx), "DynamicArray should be default constructible."
			);
			assertTrue(
				arr_st.isTriviallyZeroInitializable(ctx),
				"DynamicArray should be trivially zero-initializable."
			);
			assertTrue(arr_st.isCopyable(ctx), "DynamicArray should be copyable.");
			assertFalse(
				arr_st.isTriviallyCopyable(ctx), "DynamicArray should not be trivially copyable."
			);

			// Test with non-copyable element.
			const auto void_type        = getVoidType();
			const auto non_copyable_arr = ctx.query<QueryDynamicArrayType>(st(void_type));
			assertFalse(
				st(non_copyable_arr).isCopyable(ctx), "Array of void should not be copyable."
			);
			assertTrue(
				st(non_copyable_arr).isDefaultConstructible(ctx),
				"Array of void should be default constructible."
			);
		});
	}

	/**
	 * Test that static arrays with different properties are treated as different types
	 * and that they are correctly cast and handled.
	 */
	void simpleStaticArray() {
		const auto int_16
			= getIntegralTypeNoContext(16, compiler::tsh::IntegralAbstractType::Signedness::Signed);
		const auto int_32
			= getIntegralTypeNoContext(32, compiler::tsh::IntegralAbstractType::Signedness::Signed);

		const auto arr_1 = query::entryPoint<QueryStaticArrayType>({ st(int_16), 10 });

		assertTrue(arr_1.getKind() == StaticArray, "StaticArray should have kind StaticArray.");
		assertTrue(arr_1.getElementType() == st(int_16), "Element type should be as constructed.");
		assertTrue(arr_1.getSize() == 10, "Size should be as constructed.");

		const AbstractType            type_arr = arr_1;
		const StaticArrayAbstractType arr_2    = type_arr;
		assertTrue(arr_2.getKind() == StaticArray, "StaticArray should survive casting.");
		assertTrue(arr_2.getSize() == 10, "Size should survive casting.");

		const auto arr_3 = query::entryPoint<QueryStaticArrayType>({ st(int_16), 10 });
		assertTrue(
			arr_1 == arr_3, "StaticArrays with the same element type and size should be equal."
		);

		const auto arr_4_diff_type = query::entryPoint<QueryStaticArrayType>({ st(int_32), 10 });
		assertTrue(
			arr_1 != arr_4_diff_type,
			"StaticArrays with different element types should be different."
		);

		const auto arr_5_diff_size = query::entryPoint<QueryStaticArrayType>({ st(int_16), 20 });
		assertTrue(
			arr_1 != arr_5_diff_size, "StaticArrays with different sizes should be different."
		);

		const auto arr_6_diff_mut
			= query::entryPoint<QueryStaticArrayType>({ st(int_16, true), 10 });
		assertTrue(
			arr_1 != arr_6_diff_mut,
			"StaticArrays with element with different mutability should be different."
		);

		assertTrue(arr_1.hasNoOpDestructor(), "StaticArray of Ints should have a no-op destructor.");

		const auto str_type = getStringType();
		const auto arr_str  = query::entryPoint<QueryStaticArrayType>({ st(str_type), 5 });
		assertFalse(
			arr_str.hasNoOpDestructor(), "StaticArray of Strings should not have a no-op destructor."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(arr_1.carriesInformation(ctx), "Array of ints should carry information");
			const auto unit = getUnitType();

			const auto unit_array = ctx.query<QueryStaticArrayType>({ st(unit), 10 });
			assertFalse(
				unit_array.carriesInformation(ctx), "Array of units should not carry information"
			);

			const auto empty_array = ctx.query<QueryStaticArrayType>({ st(int_16), 0 });
			assertFalse(
				empty_array.carriesInformation(ctx), "Empty array should not carry information"
			);

			const auto arr_st_trivial = st(arr_1);
			assertTrue(
				arr_st_trivial.isDefaultConstructible(ctx),
				"StaticArray of Ints should be default constructible."
			);
			assertTrue(
				arr_st_trivial.isTriviallyZeroInitializable(ctx),
				"StaticArray of Ints should be trivially zero-initializable."
			);
			assertTrue(arr_st_trivial.isCopyable(ctx), "StaticArray of Ints should be copyable.");
			assertTrue(
				arr_st_trivial.isTriviallyCopyable(ctx),
				"StaticArray of Ints should be trivially copyable."
			);

			const auto list_type = ctx.query<QueryDynamicArrayType>({ st(str_type) });
			const auto arr_st_complex
				= ctx.query<QueryStaticArrayType>({ .element_type = st(list_type), .size = 2 });
			assertTrue(
				arr_st_complex.isDefaultConstructible(ctx),
				"StaticArray of Lists should be default constructible."
			);
			assertTrue(
				arr_st_complex.isTriviallyZeroInitializable(ctx),
				"StaticArray of Lists should be trivially zero-initializable."
			);
			assertTrue(arr_st_complex.isCopyable(ctx), "StaticArray of Lists should be copyable.");
			assertFalse(
				arr_st_complex.isTriviallyCopyable(ctx),
				"StaticArray of Lists should not be trivially copyable."
			);

			// Test non-default-constructible static array with ref element)
			const auto ref_type    = SymbolType(int_16, ReferenceKind::Ref, Mutability::Immutable);
			const auto arr_of_refs = ctx.query<QueryStaticArrayType>({ ref_type, 5 });
			assertFalse(
				st(arr_of_refs).isDefaultConstructible(ctx),
				"StaticArray of references should not be default constructible."
			);
			assertTrue(
				st(arr_of_refs).isTriviallyCopyable(ctx),
				"StaticArray of references should be trivially copyable."
			);
		});
	}

	/**
	 * Test that tuples with different components are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleTuple() {
		using enum IntegralAbstractType::Signedness;
		const auto int_16 = getIntegralTypeNoContext(16, Signed);
		const auto int_32 = getIntegralTypeNoContext(32, Signed);

		const auto tup_1 = query::entryPoint<QueryTupleType>({ { st(int_16), st(int_32) } });

		assertTrue(
			tup_1.getComponents() == std::vector({ st(int_16), st(int_32) }),
			"Component types should be as constructed."
		);
		assertTrue(tup_1.getKind() == Tuple, "Tuple should have kind Tuple.");

		const AbstractType      type_tup = tup_1;
		const TupleAbstractType tup_2    = type_tup;
		assertTrue(tup_2.getKind() == Tuple, "Tuple should survive casting.");

		const auto tup_3 = query::entryPoint<QueryTupleType>({ { st(int_16), st(int_32) } });
		assertTrue(tup_1 == tup_3, "Tuples constructed the same way should be equal.");

		const auto tup_4 = query::entryPoint<QueryTupleType>({ { st(int_32), st(int_32) } });
		assertTrue(tup_1 != tup_4, "Tuples with different types should be different.");

		const auto tup_5 = query::entryPoint<QueryTupleType>({ { st(int_16, true), st(int_32) } });
		assertTrue(tup_1 != tup_5, "Tuples with different mutability should be different.");

		assertTrue(
			tup_1.hasNoOpDestructor() && tup_4.hasNoOpDestructor() && tup_5.hasNoOpDestructor(),
			"Tuples of Ints should have no op destructors."
		);

		const auto str   = getStringType();
		const auto tup_6 = query::entryPoint<QueryTupleType>({ { st(int_16), st(str) } });
		assertFalse(
			tup_6.hasNoOpDestructor(), "Tuple with String should not have no op destructor."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto tup_st_trivial = st(tup_1);
			assertTrue(
				tup_st_trivial.isDefaultConstructible(ctx),
				"Tuple of Ints should be default constructible."
			);
			assertTrue(
				tup_st_trivial.isTriviallyZeroInitializable(ctx),
				"Tuple of Ints should be trivially zero-initializable."
			);
			assertTrue(tup_st_trivial.isCopyable(ctx), "Tuple of Ints should be copyable.");
			assertTrue(
				tup_st_trivial.isTriviallyCopyable(ctx),
				"Tuple of Ints should be trivially copyable."
			);

			const auto list_type      = ctx.query<QueryDynamicArrayType>({ st(int_32) });
			const auto tup_st_complex = ctx.query<QueryTupleType>({ { st(list_type) } });
			assertTrue(
				tup_st_complex.isDefaultConstructible(ctx),
				"Tuple with List should be default constructible."
			);
			assertTrue(
				tup_st_complex.isTriviallyZeroInitializable(ctx),
				"Tuple with List should be trivially zero-initializable."
			);
			assertTrue(tup_st_complex.isCopyable(ctx), "Tuple with List should be copyable.");
			assertFalse(
				tup_st_complex.isTriviallyCopyable(ctx),
				"Tuple with List should not be trivially copyable."
			);

			// Test non-default-constructible tuple with Ref element.
			const auto ref_type    = SymbolType(int_16, ReferenceKind::Ref, Mutability::Immutable);
			const auto tup_of_refs = ctx.query<QueryTupleType>({ { ref_type, st(int_32) } });
			assertFalse(
				st(tup_of_refs).isDefaultConstructible(ctx),
				"Tuple with reference should not be default constructible."
			);
			assertTrue(
				st(tup_of_refs).isTriviallyCopyable(ctx),
				"Tuple with reference should not be trivially copyable."
			);
		});
	}

	/**
	 * Test that variants with different components are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleVariant() {
		using enum IntegralAbstractType::Signedness;
		const auto int_16 = getIntegralTypeNoContext(16, Signed);
		const auto int_32 = getIntegralTypeNoContext(32, Signed);

		const auto var_1 = query::entryPoint<QueryVariantType>({ { st(int_16), st(int_32) } });

		assertTrue(
			var_1.getUnderlyingTypes() == std::vector({ st(int_16), st(int_32) }),
			"Underlying types should be as constructed."
		);
		assertTrue(var_1.getKind() == Variant, "Variant should have kind Variant.");

		const AbstractType        type_var = var_1;
		const VariantAbstractType var_2    = type_var;
		assertTrue(var_2.getKind() == Variant, "Tuple should survive casting.");

		const auto var_3 = query::entryPoint<QueryVariantType>({ { st(int_16), st(int_32) } });
		assertTrue(var_1 == var_3, "Variants constructed the same way should be equal.");

		const auto var_4 = query::entryPoint<QueryVariantType>({ { st(int_32), st(int_32) } });
		assertTrue(var_1 != var_4, "Variants with different underlying types should be different.");

		assertTrue(
			var_1.hasNoOpDestructor() && var_4.hasNoOpDestructor(),
			"Variants of Ints should have no op destructors."
		);

		const auto str   = getStringType();
		const auto var_5 = query::entryPoint<QueryVariantType>({ { st(int_16), st(str) } });
		assertFalse(
			var_5.hasNoOpDestructor(), "Variant with String should not have no op destructor."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto var_st = st(var_1);
			assertFalse(
				var_st.isDefaultConstructible(ctx),
				"Variant of Ints should NOT be default constructible."
			);
			assertFalse(
				var_st.isTriviallyZeroInitializable(ctx),
				"Variant should not be trivially zero-initializable (variant tag)."
			);
			assertTrue(var_st.isCopyable(ctx), "Variant of Ints should be copyable.");
			assertTrue(
				var_st.isTriviallyCopyable(ctx),
				"Variant of Ints should be trivially copyable (if all components are)."
			);
		});
	}

	/**
	 * Test that function pointers and objects are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleFunction() {
		using enum IntegralAbstractType::Signedness;
		const auto int_16 = getIntegralTypeNoContext(16, Signed);
		const auto int_32 = getIntegralTypeNoContext(32, Signed);

		const auto fun_1
			= query::entryPoint<QueryFunctionType>({ { st(int_16), st(int_32) }, st(int_32) });

		assertTrue(
			fun_1.getParameterTypes() == std::vector({ st(int_16), st(int_32) }),
			"Parameter types should be as constructed."
		);
		assertTrue(fun_1.getResultType() == st(int_32), "Result type should be as constructed.");
		assertTrue(!fun_1.isPure(), "Purity should be as constructed.");
		assertTrue(!fun_1.isFree(), "Freedom should be as constructed.");

		const AbstractType         type_fun = fun_1;
		const FunctionAbstractType fun2     = type_fun;
		assertTrue(fun2.getKind() == Function, "Function should survive casting.");

		const auto fun_identical
			= query::entryPoint<QueryFunctionType>({ { st(int_16), st(int_32) }, st(int_32) });
		assertTrue(
			fun_1 == fun_identical, "Function types constructed the same way should be equal."
		);
		const auto fun_different_input
			= query::entryPoint<QueryFunctionType>({ { st(int_32), st(int_32) }, st(int_32) });
		assertTrue(
			fun_1 != fun_different_input, "Functions of different input types should be different."
		);
		const auto fun_different_output
			= query::entryPoint<QueryFunctionType>({ { st(int_16), st(int_32) }, st(int_16) });
		assertTrue(
			fun_1 != fun_different_output, "Functions of different output types should be different."
		);
		const auto fun_different_flags = query::entryPoint<QueryFunctionType>(
			{ { st(int_16), st(int_32) }, st(int_32), true, true }
		);
		assertTrue(
			fun_1 != fun_different_flags, "Functions with different flags should be different."
		);

		const AbstractType         type_fun_different_flags = fun_different_flags;
		const FunctionAbstractType fun_different_flags_2    = type_fun_different_flags;
		assertTrue(
			fun_different_flags == fun_different_flags_2, "Function flags should survive casting."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto fun_st = st(fun_1);
			assertFalse(
				fun_st.isDefaultConstructible(ctx),
				"Function type should not be default constructible."
			);
			assertFalse(
				fun_st.isTriviallyZeroInitializable(ctx),
				"Function type should not be trivially zero-initializable."
			);
			assertTrue(fun_st.isCopyable(ctx), "Function type should be copyable.");
		});
	}

	void simpleLanguageElements() {
		const auto nspace   = getNamespaceType();
		const auto nspace_2 = getNamespaceType();

		assertTrue(nspace == nspace_2, "There shouldn't be multiple different Namespace types.");

		assertTrue(nspace.getKind() == Namespace, "NamespaceType should have kind Meta.");
		assertTrue(nspace.hasNoOpDestructor(), "NamespaceType should have no op destructor.");

		const AbstractType          nspace_type = nspace;
		const NamespaceAbstractType nspace_3    = nspace_type;
		assertTrue(nspace_3.getKind() == Namespace, "NamespaceType should survive casting.");

		const auto module   = getModuleType();
		const auto module_2 = getModuleType();

		assertTrue(module == module_2, "There shouldn't be multiple different Module types.");

		assertTrue(module.getKind() == Module, "ModuleType should have kind Meta.");
		assertFalse(module.hasNoOpDestructor(), "ModuleType should not have no op destructor.");

		const AbstractType       module_type = module;
		const ModuleAbstractType module_3    = module_type;
		assertTrue(module_3.getKind() == Module, "ModuleType should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			assertFalse(st(nspace).isCopyable(ctx), "Namespace should not be copyable.");
			assertFalse(
				st(nspace).isDefaultConstructible(ctx),
				"Namespace should not be default constructible."
			);

			assertFalse(st(module).isCopyable(ctx), "Module should not be copyable.");
			assertFalse(
				st(module).isDefaultConstructible(ctx), "Module should not be default constructible."
			);
		});
	}

	void simpleMeta() {
		const auto meta   = getMetaType();
		const auto meta_2 = getMetaType();

		assertTrue(meta == meta_2, "There shouldn't be multiple different 'type' types.");

		assertTrue(meta.getKind() == Meta, "MetaType should have kind Meta.");
		assertTrue(meta.hasNoOpDestructor(), "MetaType should have no op destructor.");

		const AbstractType     meta_type = meta;
		const MetaAbstractType met_3     = meta_type;
		assertTrue(met_3.getKind() == Meta, "MetaType should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				st(meta).isDefaultConstructible(ctx),
				"Meta type should be default constructible (e.g. to void)."
			);
			assertTrue(st(meta).isCopyable(ctx), "Meta type should be copyable.");
			assertFalse(
				st(meta).isTriviallyZeroInitializable(ctx),
				"Meta type should not be trivially zero initializable."
			);
			assertTrue(st(meta).isTriviallyCopyable(ctx), "Meta type should be trivially copyable.");
		});
	}

	void simpleTypeTemplate() {
		query::utils::withContextDo([&](query::Context& ctx) -> void {
			const auto list_template_type
				= ctx.query<QueryTypeTemplateType>({ TypeTemplateAbstractType::BuiltinKind::List });

			assertTrue(
				list_template_type.getKind() == TypeTemplate,
				"Type template for List should have kind TypeTemplate."
			);

			assertTrue(
				list_template_type.carriesInformation(ctx),
				"Type templates should not carry information."
			);

			assertTrue(
				list_template_type.hasNoOpDestructor(),
				"Type templates should have a no-op destructor."
			);

			assertFalse(
				st(list_template_type).isCopyable(ctx), "TypeTemplate should not be copyable."
			);
			assertFalse(
				st(list_template_type).isDefaultConstructible(ctx),
				"TypeTemplate should not be default constructible."
			);

			const auto list_template_type_2
				= ctx.query<QueryTypeTemplateType>({ TypeTemplateAbstractType::BuiltinKind::List });
			assertTrue(
				list_template_type == list_template_type_2,
				"Queries for the same type template should return the same type object."
			);


			const auto i64_type
				= getIntegralType(ctx, 64, IntegralAbstractType::Signedness::Signed);
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const auto i64_st             = st(i64_type);
			const auto i32_st             = st(i32_type);
			const auto instantiated_abs   = list_template_type.instantiate(ctx, i64_st);
			const auto instantiated_abs_2 = list_template_type_2.instantiate(ctx, i64_st);
			const auto instantiated_abs_3 = list_template_type_2.instantiate(ctx, i32_st);
			const auto expected           = ctx.query<QueryDynamicArrayType>({ i64_st });
			const auto expected_2         = ctx.query<QueryDynamicArrayType>({ i32_st });
			assertTrue(
				expected == instantiated_abs,
				"Instantiating list type template should produce a dynamic array of i64"
			);
			assertTrue(
				expected_2 == instantiated_abs_3,
				"Instantiating list type template should produce a dynamic array of i32"
			);
			assertTrue(
				instantiated_abs_2 == instantiated_abs,
				"Two same type templates instantiated with the same type should produce the same "
				"type"
			);
			assertFalse(
				instantiated_abs_3 == instantiated_abs,
				"Two same type templates instantiated with different types should produce a "
				"different "
				"type"
			);
		});
	}

	void simpleExpressionType() {
		using enum IntegralAbstractType::Signedness;

		const auto void_i = getVoidType();
		const auto int_i  = getIntegralTypeNoContext(8, Signed);

		const ExpressionType int_desc(st(int_i), ValueCategory(PrimaryCategory::Local));

		try {
			ExpressionType<IntegralAbstractType>{
				st(void_i),
				ValueCategory(PrimaryCategory::Local),
			};
			fail("Created IntegralDesc for Void type.");
		} catch (const base::Panic&) {
			// expected
		}

		ExpressionType{ st(void_i), ValueCategory(PrimaryCategory::Local) };

		const ExpressionType really_int_desc = int_desc;

		assertTrue(int_desc.getType().getKind() == Integral, "IntDesc type is not int");
		assertTrue(
			really_int_desc.getType().getKind() == Integral,
			"IntDesc type after conversion to ExpressionType is not int"
		);
	}

	void simpleValueCategory() {
		using enum ValueSemanticsOptions;

		const auto vc = ValueCategory(
			PrimaryCategory::Local, false, MOVE | COPY | REINIT | USE | DESTROY, ValueSemantics()
		);
		assertTrue(
			vc.getCategory() == PrimaryCategory::Local,
			"ValueCategory constructor should initialize unchanged category value."
		);
		assertTrue(
			!vc.isPure(), "ValueCategory constructor should initialize unchanged is_pure value."
		);
		assertTrue(
			vc.getAllowsSemantic() == (MOVE | COPY | REINIT | USE | DESTROY),
			"ValueCategory constructor should initialize unchanged allowsSemantic value."
		);
		assertTrue(
			vc.getForceSemantic() == ValueSemantics(),
			"ValueCategory constructor should initialize unchanged forceSemantic value."
		);

		const auto vc_1 = ValueCategory(
			PrimaryCategory::Local, false, MOVE | COPY | REINIT | USE | DESTROY, ValueSemantics()
		);
		assertTrue(vc == vc_1, "Value categories constructed the same way should be equal.");

		const auto vc_2
			= ValueCategory(PrimaryCategory::Local, false, COPY | REINIT | USE, ValueSemantics());
		assertTrue(
			vc_1.contains(vc_2),
			"Value category with full allows_semantic should contain same value category with "
			"subset of allowed semantics."
		);
	}

	void simpleImplicitCoercibility() {
		using enum IntegralAbstractType::Signedness;

		const auto int_2 = getIntegralTypeNoContext(8U * (1 << 2), Signed);
		const auto int_3 = getIntegralTypeNoContext(8U * (1 << 3), Signed);
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_2, int_3 }),
			"Smaller int should be coercible into a bigger one."
		);

		assertTrue(
			!query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_3, int_2 }),
			"Bigger int should not be coercible into a smaller one."
		);

		const auto u_int_2 = getIntegralTypeNoContext(8U * (1 << 2), Unsigned);
		const auto u_int_3 = getIntegralTypeNoContext(8U * (1 << 3), Unsigned);
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ u_int_2, u_int_3 }),
			"Smaller unsigned int should be coercible into a bigger one."
		);

		assertTrue(
			!query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ u_int_3, u_int_2 }),
			"Bigger unsigned int should not be coercible into a smaller one."
		);

		{
			assertFalse(
				query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ u_int_3, int_3 }),
				"Unsigned int should not be coercible into a signed int of the same size (positive "
				"values range is bigger so the coercion is lossy)."
			);

			assertFalse(
				query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ u_int_2, int_2 }),
				"Unsigned int should not be coercible into a signed int of the same size (positive "
				"values range is bigger so the coercion is lossy)."
			);
		}

		{
			assertFalse(
				query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_2, u_int_2 }),
				"Signed int should not be coercible into an unsigned int of the same size "
				"(coercion is lossy for negative values)."
			);

			assertFalse(
				query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_3, u_int_3 }),
				"Signed int should not be coercible into an unsigned int of the same size "
				"(coercion is lossy for negative values)."
			);

			assertFalse(
				query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_3, u_int_2 }),
				"Signed int should not be coercible into an unsigned int of the same size "
				"(coercion is lossy for negative values)."
			);
		}

		const auto void_type = getVoidType();
		assertTrue(
			!query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ void_type, int_2 }),
			"Void should not be coercible to anything."
		);

		const auto i2_const
			= ExpressionType(st(int_2), ValueCategory(PrimaryCategory::Local, true, {}, {}));
		const auto i2_mut
			= ExpressionType(st(int_2, true), ValueCategory(PrimaryCategory::Local, true, {}, {}));
		const auto i3_const
			= ExpressionType(st(int_3), ValueCategory(PrimaryCategory::Local, true, {}, {}));

		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnExpressionType>({ i2_const, i3_const }),
			"Smaller int value should be coercible into a bigger one."
		);
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnExpressionType>({ i2_mut, i2_const }),
			"Mutable value should be coercible to an immutable one."
		);
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnExpressionType>({ i2_mut, i3_const }),
			"Mutable value should be coercible to a bigger, immutable one."
		);
		//
		// @TODO: #1488 Deal with this conversion on integer literals
		// assertTrue(
		// 	!query::entryPoint<QueryImplicitCoercibilityOnExpressionType>({ i2_const, i2_mut }),
		// 	"Immutable value should not be coercible to a mutable one."
		// );
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/tsh/tests/")
