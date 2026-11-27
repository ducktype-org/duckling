// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <any>
#include <tuple>

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
		TESTER_ADD_TEST(simpleStaticArray);
		TESTER_ADD_TEST(simpleTuple);
		TESTER_ADD_TEST(simpleVariant);
		TESTER_ADD_TEST(simpleFunction);
		TESTER_ADD_TEST(simpleLanguageElements);
		TESTER_ADD_TEST(simpleMeta);
		TESTER_ADD_TEST(simpleExpressionType);
		TESTER_ADD_TEST(simpleValueCategory);
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
			assertTrue(
				void_1.isTriviallyDestructible(ctx), "Void should be trivially destructible."
			);
			assertTrue(
				unit_1.isTriviallyDestructible(ctx), "Unit should be trivially destructible."
			);

			const auto unit_vt = vt(unit_1);
			assertTrue(unit_vt.isDefaultConstructible(ctx), "Unit should be default constructible.");
			assertFalse(
				unit_vt.isTriviallyZeroInitializable(ctx),
				"Unit shouldn't be trivially zero-initializable."
			);
			assertTrue(unit_vt.isCopyable(ctx), "Unit should be copyable.");
			assertTrue(unit_vt.isTriviallyCopyable(ctx), "Unit should be trivially copyable.");

			const auto void_vt = vt(void_1);
			assertFalse(
				void_vt.isDefaultConstructible(ctx), "Void should be default constructible."
			);
			assertFalse(
				void_vt.isTriviallyZeroInitializable(ctx),
				"Void should be trivially zero-initializable."
			);
			assertFalse(void_vt.isCopyable(ctx), "Void should be copyable.");
			assertFalse(void_vt.isTriviallyCopyable(ctx), "void should be trivially copyable.");
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
			const auto bool_vt = vt(bool_1);
			assertTrue(bool_vt.isDefaultConstructible(ctx), "Bool should be default constructible.");
			assertTrue(
				bool_vt.isTriviallyZeroInitializable(ctx),
				"Bool should be trivially zero-initializable."
			);
			assertTrue(bool_vt.isCopyable(ctx), "Bool should be copyable.");
			assertTrue(bool_vt.isTriviallyCopyable(ctx), "Bool should be trivially copyable.");

			const auto byte_vt = vt(byte_1);
			assertTrue(byte_vt.isDefaultConstructible(ctx), "Byte should be default constructible.");
			assertTrue(
				byte_vt.isTriviallyZeroInitializable(ctx),
				"Byte should be trivially zero-initializable."
			);
			assertTrue(byte_vt.isCopyable(ctx), "Byte should be copyable.");
			assertTrue(byte_vt.isTriviallyCopyable(ctx), "Byte should be trivially copyable.");

			const auto char_vt = vt(char_1);
			assertTrue(char_vt.isDefaultConstructible(ctx), "Char should be default constructible.");
			assertTrue(
				char_vt.isTriviallyZeroInitializable(ctx),
				"Char should be trivially zero-initializable."
			);
			assertTrue(char_vt.isCopyable(ctx), "Char should be copyable.");
			assertTrue(char_vt.isTriviallyCopyable(ctx), "Char should be trivially copyable.");
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

			query::utils::withContextDo([&](query::Context& ctx) {
				assertTrue(
					int_1.isTriviallyDestructible(ctx) && int_u.isTriviallyDestructible(ctx),
					"Ints should be trivially destructible."
				);
			});
		}

		assertTrue(
			getIntegralTypeNoContext(8, Signed) != getIntegralTypeNoContext(16, Signed),
			"Ints of different sizes should be different."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto int_vt = vt(getIntegralType(ctx, 32, Signed));
			assertTrue(int_vt.isDefaultConstructible(ctx), "Int should be default constructible.");
			assertTrue(
				int_vt.isTriviallyZeroInitializable(ctx),
				"Int should be trivially zero-initializable."
			);
			assertTrue(int_vt.isCopyable(ctx), "Int should be copyable.");
			assertTrue(int_vt.isTriviallyCopyable(ctx), "Int should be trivially copyable.");

			const auto int_ref_vt = int_vt.toRef(ViewAccess::ReadOnly);
			assertFalse(
				int_ref_vt.isDefaultConstructible(ctx),
				"Reference to Int should NOT be default constructible."
			);
			assertFalse(
				int_ref_vt.isTriviallyZeroInitializable(ctx),
				"Reference to Int should NOT be trivially zero-initializable."
			);
			assertTrue(int_ref_vt.isCopyable(ctx), "Reference to Int should be copyable.");
			assertTrue(
				int_ref_vt.isTriviallyCopyable(ctx), "Reference to Int should be trivially copyable."
			);

			const auto int_box_vt = int_vt.toBox();
			assertFalse(
				int_box_vt.isDefaultConstructible(ctx),
				"Box of Int should NOT be default constructible."
			);
			assertFalse(
				int_box_vt.isTriviallyZeroInitializable(ctx),
				"Box of Int should NOT be trivially zero-initializable."
			);
			assertTrue(int_box_vt.isCopyable(ctx), "Box of Int should be copyable.");
			assertFalse(
				int_box_vt.isTriviallyCopyable(ctx), "Box of Int should not be trivially copyable."
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

			query::utils::withContextDo([&](query::Context& ctx) {
				assertTrue(
					float_1.isTriviallyDestructible(ctx), "Floats should be trivially destructible."
				);
			});
		}

		assertTrue(
			getFloatTypeNoContext(32) != getFloatTypeNoContext(64),
			"Floats of different sizes should be different."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto float_vt = vt(getFloatType(ctx, 64));
			assertTrue(
				float_vt.isDefaultConstructible(ctx), "Float should be default constructible."
			);
			assertTrue(
				float_vt.isTriviallyZeroInitializable(ctx),
				"Float should be trivially zero-initializable."
			);
			assertTrue(float_vt.isCopyable(ctx), "Float should be copyable.");
			assertTrue(float_vt.isTriviallyCopyable(ctx), "Float should be trivially copyable.");
		});
	}

	static ValueType<> vt(const AbstractType abstract_type) {
		return ValueType<>::direct(abstract_type);
	}

	/**
	 * Test that Raw Pointer and Pointer types correctly cast
	 * between each other and retain AbstractTypermation as expected.
	 */
	void simplePointer() {
		const auto raw_1 = getRawPointerType(ViewAccess::ReadOnly);
		assertTrue(raw_1.getKind() == RawPointer, "Raw Pointer should have kind RawPointer.");
		const auto raw_2 = getRawPointerType(ViewAccess::ReadWrite);
		assertTrue(
			raw_2.getKind() == RawPointer, "Mutable Raw Pointer should have kind RawPointer."
		);
		assertTrue(raw_1 != raw_2, "Immutable and mutable Raw Pointers should be different.");
		const auto raw_3 = getRawPointerType(ViewAccess::ReadOnly);
		assertTrue(raw_1 == raw_3, "There should be only one immutable Raw Pointer.");
		const auto raw_4 = getRawPointerType(ViewAccess::ReadWrite);
		assertTrue(raw_2 == raw_4, "There should be only one mutable Raw Pointer.");

		const AbstractType           type_raw = raw_1;
		const RawPointerAbstractType raw_5    = type_raw;
		assertTrue(raw_5.getKind() == RawPointer, "Raw Pointer should survive casting.");

		const auto ptr_1 = query::entryPoint<QueryPointerType>({ .pointee = vt(raw_1),
		                                                         .access  = ViewAccess::ReadOnly });
		assertTrue(ptr_1.getKind() == Pointer, "Pointer should have kind Pointer.");
		const auto ptr_2 = query::entryPoint<QueryPointerType>({ .pointee = vt(raw_1),
		                                                         .access  = ViewAccess::ReadOnly });
		assertTrue(ptr_1 == ptr_2, "Pointers to the same type and mutability should be equal.");
		const auto ptr_3 = query::entryPoint<QueryPointerType>({ .pointee = vt(raw_2),
		                                                         .access  = ViewAccess::ReadOnly });
		assertTrue(ptr_1 != ptr_3, "Pointers to different types should be different.");
		const auto ptr_4 = query::entryPoint<QueryPointerType>({ .pointee = vt(raw_1),
		                                                         .access = ViewAccess::ReadWrite });
		assertTrue(ptr_1 != ptr_4, "A `ptr` and a `ptr mut` should be different.");

		const AbstractType        type_ptr = ptr_4;
		const PointerAbstractType ptr_5    = type_ptr;
		assertTrue(
			ptr_5.getKind() == Pointer && ptr_5.getPointee() == ptr_4.getPointee(),
			"Pointer should survive casting."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto raw_vt = vt(raw_1);
			assertTrue(
				raw_vt.isDefaultConstructible(ctx), "RawPointer should be default constructible."
			);
			assertTrue(
				raw_vt.isTriviallyZeroInitializable(ctx), "RawPointer should be zero initializable."
			);
			assertTrue(raw_vt.isCopyable(ctx), "RawPointer should be copyable.");
			assertTrue(raw_vt.isTriviallyCopyable(ctx), "RawPointer should be trivially copyable.");

			const auto pointer_vt = vt(ptr_1);
			assertTrue(
				pointer_vt.isDefaultConstructible(ctx), "Pointer should be default constructible."
			);
			assertTrue(
				pointer_vt.isTriviallyZeroInitializable(ctx),
				"Pointer should be trivially zero-initializable."
			);
			assertTrue(pointer_vt.isCopyable(ctx), "Pointer should be copyable.");
			assertTrue(pointer_vt.isTriviallyCopyable(ctx), "Pointer should be trivially copyable.");
		});
	}

	void simpleStaticArray() {
		const auto int_16
			= getIntegralTypeNoContext(16, compiler::tsh::IntegralAbstractType::Signedness::Signed);
		const auto int_32
			= getIntegralTypeNoContext(32, compiler::tsh::IntegralAbstractType::Signedness::Signed);

		const auto arr_1 = query::entryPoint<QueryStaticArrayType>({ vt(int_16), 10 });

		assertTrue(arr_1.getKind() == StaticArray, "StaticArray should have kind StaticArray.");
		assertTrue(arr_1.getElementType() == vt(int_16), "Element type should be as constructed.");
		assertTrue(arr_1.getSize() == 10, "Size should be as constructed.");

		const AbstractType            type_arr = arr_1;
		const StaticArrayAbstractType arr_2    = type_arr;
		assertTrue(arr_2.getKind() == StaticArray, "StaticArray should survive casting.");
		assertTrue(arr_2.getSize() == 10, "Size should survive casting.");

		const auto arr_3 = query::entryPoint<QueryStaticArrayType>({ vt(int_16), 10 });
		assertTrue(
			arr_1 == arr_3, "StaticArrays with the same element type and size should be equal."
		);

		const auto arr_4_diff_type = query::entryPoint<QueryStaticArrayType>({ vt(int_32), 10 });
		assertTrue(
			arr_1 != arr_4_diff_type,
			"StaticArrays with different element types should be different."
		);

		const auto arr_5_diff_size = query::entryPoint<QueryStaticArrayType>({ vt(int_16), 20 });
		assertTrue(
			arr_1 != arr_5_diff_size, "StaticArrays with different sizes should be different."
		);


		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				arr_1.isTriviallyDestructible(ctx),
				"StaticArray of Ints should be trivially destructible."
			);
			assertTrue(arr_1.carriesInformation(ctx), "Array of ints should carry information");
			const auto unit = getUnitType();

			const auto unit_array = ctx.query<QueryStaticArrayType>({ vt(unit), 10 });
			assertFalse(
				unit_array.carriesInformation(ctx), "Array of units should not carry information"
			);

			const auto empty_array = ctx.query<QueryStaticArrayType>({ vt(int_16), 0 });
			assertFalse(
				empty_array.carriesInformation(ctx), "Empty array should not carry information"
			);

			const auto arr_st_trivial = vt(arr_1);
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

			// Test non-default-constructible static array with ref element)
			const auto ref_type    = ValueType<>::ref(int_16, ViewAccess::ReadOnly);
			const auto arr_of_refs = ctx.query<QueryStaticArrayType>({ ref_type, 5 });
			assertFalse(
				vt(arr_of_refs).isDefaultConstructible(ctx),
				"StaticArray of references should not be default constructible."
			);
			assertTrue(
				vt(arr_of_refs).isTriviallyCopyable(ctx),
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

		const auto tup_1 = query::entryPoint<QueryTupleType>({ { vt(int_16), vt(int_32) } });

		assertTrue(
			tup_1.getComponents() == std::vector({ vt(int_16), vt(int_32) }),
			"Component types should be as constructed."
		);
		assertTrue(tup_1.getKind() == Tuple, "Tuple should have kind Tuple.");

		const AbstractType      type_tup = tup_1;
		const TupleAbstractType tup_2    = type_tup;
		assertTrue(tup_2.getKind() == Tuple, "Tuple should survive casting.");

		const auto tup_3 = query::entryPoint<QueryTupleType>({ { vt(int_16), vt(int_32) } });
		assertTrue(tup_1 == tup_3, "Tuples constructed the same way should be equal.");

		const auto tup_4 = query::entryPoint<QueryTupleType>({ { vt(int_32), vt(int_32) } });
		assertTrue(tup_1 != tup_4, "Tuples with different types should be different.");

		const auto tup_5 = query::entryPoint<QueryTupleType>({ { vt(int_16), vt(int_32) } });


		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				tup_1.isTriviallyDestructible(ctx) && tup_4.isTriviallyDestructible(ctx)
					&& tup_5.isTriviallyDestructible(ctx),
				"Tuples of Ints should be trivially destructible."
			);
			const auto tup_st_trivial = vt(tup_1);
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

			// Test non-default-constructible tuple with Ref element.
			const auto ref_type    = ValueType<>::ref(int_16, ViewAccess::ReadOnly);
			const auto tup_of_refs = ctx.query<QueryTupleType>({ { ref_type, vt(int_32) } });
			assertFalse(
				vt(tup_of_refs).isDefaultConstructible(ctx),
				"Tuple with reference should not be default constructible."
			);
			assertTrue(
				vt(tup_of_refs).isTriviallyCopyable(ctx),
				"Tuple with reference should not be trivially copyable."
			);

			// A tuple carries information only if one of its components does.
			assertTrue(tup_1.carriesInformation(ctx), "Tuple of ints should carry information");
			const auto unit       = getUnitType();
			const auto unit_tuple = ctx.query<QueryTupleType>({ { vt(unit), vt(unit) } });
			assertFalse(
				unit_tuple.carriesInformation(ctx), "Tuple of units should not carry information"
			);
			const auto nested_unit_tuple
				= ctx.query<QueryTupleType>({ { vt(unit_tuple), vt(unit) } });
			assertFalse(
				nested_unit_tuple.carriesInformation(ctx),
				"Nested tuple of units should not carry information"
			);
			const auto mixed_tuple = ctx.query<QueryTupleType>({ { vt(unit), vt(int_16) } });
			assertTrue(
				mixed_tuple.carriesInformation(ctx),
				"Tuple with an int component should carry information"
			);
			const auto unit_ref  = ValueType<>::ref(unit, ViewAccess::ReadOnly);
			const auto ref_tuple = ctx.query<QueryTupleType>({ { unit_ref, vt(unit) } });
			assertTrue(
				ref_tuple.carriesInformation(ctx),
				"Tuple with a reference component should carry information"
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

		const auto var_1 = query::entryPoint<QueryVariantType>({ { vt(int_16), vt(int_32) } });

		assertTrue(
			var_1.getUnderlyingTypes() == std::vector({ vt(int_16), vt(int_32) }),
			"Underlying types should be as constructed."
		);
		assertTrue(var_1.getKind() == Variant, "Variant should have kind Variant.");

		const AbstractType        type_var = var_1;
		const VariantAbstractType var_2    = type_var;
		assertTrue(var_2.getKind() == Variant, "Tuple should survive casting.");

		const auto var_3 = query::entryPoint<QueryVariantType>({ { vt(int_16), vt(int_32) } });
		assertTrue(var_1 == var_3, "Variants constructed the same way should be equal.");

		const auto var_4 = query::entryPoint<QueryVariantType>({ { vt(int_32), vt(int_32) } });
		assertTrue(var_1 != var_4, "Variants with different underlying types should be different.");

		const auto var_boxed = query::entryPoint<QueryVariantType>({
			{ vt(int_16), vt(int_32).toBox() },
		});


		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				var_1.isTriviallyDestructible(ctx) && var_4.isTriviallyDestructible(ctx),
				"Variants of Ints should be trivially destructible."
			);
			const auto var_vt = vt(var_1);
			assertFalse(
				var_vt.isDefaultConstructible(ctx),
				"Variant of Ints should NOT be default constructible."
			);
			assertFalse(
				var_vt.isTriviallyZeroInitializable(ctx),
				"Variant should not be trivially zero-initializable (variant tag)."
			);
			assertTrue(var_vt.isCopyable(ctx), "Variant of Ints should be copyable.");
			assertTrue(
				var_vt.isTriviallyCopyable(ctx),
				"Variant of Ints should be trivially copyable (if all components are)."
			);

			assertFalse(
				var_boxed.isTriviallyDestructible(ctx),
				"Variant with a boxed alternative should NOT be trivially destructible."
			);
			assertFalse(
				vt(var_boxed).isTriviallyCopyable(ctx),
				"Variant with a boxed alternative should NOT be trivially copyable."
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
			= query::entryPoint<QueryFunctionType>({ { vt(int_16), vt(int_32) }, vt(int_32) });

		assertTrue(
			fun_1.getParameterTypes() == std::vector({ vt(int_16), vt(int_32) }),
			"Parameter types should be as constructed."
		);
		assertTrue(fun_1.getResultType() == vt(int_32), "Result type should be as constructed.");
		assertTrue(!fun_1.isPure(), "Purity should be as constructed.");
		assertTrue(
			fun_1.getFunctionKind() == FunctionKind::Raw, "Function kind should be as constructed."
		);

		const AbstractType         type_fun = fun_1;
		const FunctionAbstractType fun2     = type_fun;
		assertTrue(fun2.getKind() == Function, "Function should survive casting.");

		const auto fun_identical
			= query::entryPoint<QueryFunctionType>({ { vt(int_16), vt(int_32) }, vt(int_32) });
		assertTrue(
			fun_1 == fun_identical, "Function types constructed the same way should be equal."
		);
		const auto fun_different_input
			= query::entryPoint<QueryFunctionType>({ { vt(int_32), vt(int_32) }, vt(int_32) });
		assertTrue(
			fun_1 != fun_different_input, "Functions of different input types should be different."
		);
		const auto fun_different_output
			= query::entryPoint<QueryFunctionType>({ { vt(int_16), vt(int_32) }, vt(int_16) });
		assertTrue(
			fun_1 != fun_different_output, "Functions of different output types should be different."
		);
		const auto fun_different_flags = query::entryPoint<QueryFunctionType>(
			{ { vt(int_16), vt(int_32) }, vt(int_32), true, FunctionKind::Closure }
		);
		assertTrue(
			fun_1 != fun_different_flags, "Functions with different flags should be different."
		);

		const auto fun_closure = query::entryPoint<QueryFunctionType>(
			{ { vt(int_16), vt(int_32) }, vt(int_32), false, FunctionKind::Closure }
		);
		assertTrue(fun_1 != fun_closure, "Raw functions and closures should be different.");
		assertTrue(
			fun_closure.getFunctionKind() == FunctionKind::Closure,
			"Function kind should be as constructed."
		);

		const AbstractType         type_fun_different_flags = fun_different_flags;
		const FunctionAbstractType fun_different_flags_2    = type_fun_different_flags;
		assertTrue(
			fun_different_flags == fun_different_flags_2, "Function flags should survive casting."
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto fun_vt = vt(fun_1);
			assertFalse(
				fun_vt.isDefaultConstructible(ctx),
				"Function type should not be default constructible."
			);
			assertFalse(
				fun_vt.isTriviallyZeroInitializable(ctx),
				"Function type should not be trivially zero-initializable."
			);
			assertTrue(fun_vt.isCopyable(ctx), "Function type should be copyable.");
		});
	}

	void simpleLanguageElements() {
		const auto nspace   = getNamespaceType();
		const auto nspace_2 = getNamespaceType();

		assertTrue(nspace == nspace_2, "There shouldn't be multiple different Namespace types.");

		assertTrue(nspace.getKind() == Namespace, "NamespaceType should have kind Meta.");

		const AbstractType          nspace_type = nspace;
		const NamespaceAbstractType nspace_3    = nspace_type;
		assertTrue(nspace_3.getKind() == Namespace, "NamespaceType should survive casting.");

		const auto module   = getModuleType();
		const auto module_2 = getModuleType();

		assertTrue(module == module_2, "There shouldn't be multiple different Module types.");

		assertTrue(module.getKind() == Module, "ModuleType should have kind Meta.");

		const AbstractType       module_type = module;
		const ModuleAbstractType module_3    = module_type;
		assertTrue(module_3.getKind() == Module, "ModuleType should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				nspace.isTriviallyDestructible(ctx),
				"NamespaceType should be trivially destructible."
			);
			IF_BUILD_TYPE_DEV(
				assertThrows<base::Panic>(
					[&]() { std::ignore = module.isTriviallyDestructible(ctx); },
					"Asking whether ModuleType is trivially destructible should panic."
				);
			)

			assertFalse(vt(nspace).isCopyable(ctx), "Namespace should not be copyable.");
			assertFalse(
				vt(nspace).isDefaultConstructible(ctx),
				"Namespace should not be default constructible."
			);

			assertFalse(vt(module).isCopyable(ctx), "Module should not be copyable.");
			assertFalse(
				vt(module).isDefaultConstructible(ctx), "Module should not be default constructible."
			);
		});
	}

	void simpleMeta() {
		const auto meta   = getMetaType();
		const auto meta_2 = getMetaType();

		assertTrue(meta == meta_2, "There shouldn't be multiple different 'type' types.");

		assertTrue(meta.getKind() == Meta, "MetaType should have kind Meta.");

		const AbstractType     meta_type = meta;
		const MetaAbstractType met_3     = meta_type;
		assertTrue(met_3.getKind() == Meta, "MetaType should survive casting.");

		query::utils::withContextDo([&](query::Context& ctx) {
			assertTrue(
				meta.isTriviallyDestructible(ctx), "MetaType should be trivially destructible."
			);
			assertTrue(
				vt(meta).isDefaultConstructible(ctx),
				"Meta type should be default constructible (e.g. to void)."
			);
			assertTrue(vt(meta).isCopyable(ctx), "Meta type should be copyable.");
			assertFalse(
				vt(meta).isTriviallyZeroInitializable(ctx),
				"Meta type should not be trivially zero initializable."
			);
			assertTrue(vt(meta).isTriviallyCopyable(ctx), "Meta type should be trivially copyable.");
		});
	}

	void simpleExpressionType() {
		using enum IntegralAbstractType::Signedness;

		const auto void_i = getVoidType();
		const auto int_i  = getIntegralTypeNoContext(8, Signed);

		const ExpressionType int_desc(vt(int_i), ValueCategory(PrimaryCategory::Local));

		try {
			ExpressionType<IntegralAbstractType>{
				vt(void_i),
				ValueCategory(PrimaryCategory::Local),
			};
			fail("Created IntegralDesc for Void type.");
		} catch (const base::Panic&) {
			// expected
		}

		ExpressionType{ vt(void_i), ValueCategory(PrimaryCategory::Local) };

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

		const auto temporary    = ValueCategory(PrimaryCategory::Temporary);
		const auto local        = ValueCategory(PrimaryCategory::Local);
		const auto global       = ValueCategory(PrimaryCategory::Global);
		const auto literal      = ValueCategory(PrimaryCategory::Literal);
		const auto dereferenced = ValueCategory(PrimaryCategory::Dereferenced);

		// Local, Global, Dereferenced are assignable, Temporary and Literal are not.
		assertTrue(
			local.canBeAssignedTo() && global.canBeAssignedTo() && dereferenced.canBeAssignedTo(),
			"Local, Global, Dereferenced should be assignable."
		);
		assertTrue(
			!temporary.canBeAssignedTo() && !literal.canBeAssignedTo(),
			"Temporary and Literal should be not assignable."
		);

		// Local, Global, Dereferenced are addressable, Temporary and Literal are not.
		assertTrue(
			local.addressable() && global.addressable() && dereferenced.addressable(),
			"Local, Global, Dereferenced should be addressable."
		);
		assertTrue(
			!temporary.addressable() && !literal.addressable(),
			"Temporary and Literal should not be addressable."
		);

		// Only values that own themselves - a Local and a Temporary - may be moved out of.
		assertTrue(local.isMovableFrom(), "Local should be a valid `move` operand.");
		assertTrue(temporary.isMovableFrom(), "Temporary should be a valid `move` operand.");
		assertTrue(
			!global.isMovableFrom() && !literal.isMovableFrom() && !dereferenced.isMovableFrom(),
			"A non-owned value should not be a valid `move` operand."
		);

		// `move` is not forced unless it's requested.
		assertTrue(
			!local.mustMove() && !temporary.mustMove(), "Default categories should not force a move."
		);
		const auto forced_move = ValueCategory(
			PrimaryCategory::Local, false, MOVE | COPY | REINIT | USE | DESTROY, MOVE
		);
		assertTrue(forced_move.mustMove(), "force_semantic with MOVE should mustMove().");
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/tsh/tests/")
