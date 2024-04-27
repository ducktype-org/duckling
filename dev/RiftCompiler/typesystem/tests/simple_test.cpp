#include <algorithm>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>

using namespace ts;

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
class SimpleTypeSystemTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleTypeSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("TypeSystem simple interface test") {
		TESTER_ADD_TEST(trivial_cast_and_assignment);
		TESTER_ADD_TEST(simple_void_and_unit);
		TESTER_ADD_TEST(simple_byte_sized);
		TESTER_ADD_TEST(simple_ints);
		TESTER_ADD_TEST(simple_floats);
		TESTER_ADD_TEST(simple_pointer);
		TESTER_ADD_TEST(simple_meta);
		TESTER_ADD_TEST(simple_type_desc);
		TESTER_ADD_TEST(simple_function);
		TESTER_ADD_TEST(simple_value_category);
	}

private:
	using enum Kind;

	/**
	 * Test that the specialized TypeInfo to TypeInfo dynamic cast works as intended.
	 * Also, test that assignment works.
	 */
	void trivial_cast_and_assignment() {
		const TypeInfo type_1{ query::entryPoint<QueryVoidType>({}) };
		TypeInfo       type_2 = type_1;
		assert(type_1 == type_2, "The trivial dynamic cast should not change any objects.");
		type_2 = query::entryPoint<QueryUnitType>({});
		assert(type_1 != type_2, "Assignment on TypeInfo should change the target object.");
	}

	/**
	 * Test that there is only one void and one unit type, and that they are correctly cast.
	 */
	void simple_void_and_unit() {
		const auto void_1 = query::entryPoint<QueryVoidType>({});
		const auto void_2 = query::entryPoint<QueryVoidType>({});
		assert(void_1 == void_2, "There should only be one Void type.");
		assert(void_1.getKind() == Void, "Void type should have kind Void.");

		const auto unit_1 = query::entryPoint<QueryUnitType>({});
		const auto unit_2 = query::entryPoint<QueryUnitType>({});
		assert(unit_1 == unit_2, "There should only be one Unit type.");
		assert(unit_1.getKind() == Unit, "Unit type should have kind Unit.");

		assert(void_1 != unit_1, "Void and Unit should be different types.");

		const TypeInfo type_void = void_1;
		const VoidInfo void_3    = type_void;
		assert(void_3.getKind() == Void, "Void should survive casting.");

		const TypeInfo type_unit = unit_1;
		const UnitInfo unit_3    = type_unit;
		assert(unit_3.getKind() == Unit, "Unit should survive casting.");
	}

	/**
	 * Test that there are three unique byte-sized types, and that they are correctly cast.
	 */
	void simple_byte_sized() {
		const auto byte_1 = query::entryPoint<QueryByteType>({});
		assert(byte_1.getKind() == Byte, "Byte type should have kind Byte.");
		const auto bool_1 = query::entryPoint<QueryBoolType>({});
		assert(bool_1.getKind() == Bool, "Bool type should have kind Bool.");
		const auto char_1 = query::entryPoint<QueryCharType>({});
		assert(char_1.getKind() == Char, "Char type should have kind Char.");

		assert(
			byte_1 != bool_1 && bool_1 != char_1 && char_1 != byte_1,
			"All byte-sized types should be different."
		);

		const auto byte_2 = query::entryPoint<QueryByteType>({});
		const auto bool_2 = query::entryPoint<QueryBoolType>({});
		const auto char_2 = query::entryPoint<QueryCharType>({});

		assert(
			byte_1 == byte_2 && bool_1 == bool_2 && char_1 == char_2,
			"Byte-sized types of the same kind should be equal."
		);

		const TypeInfo type_byte = byte_1;
		const ByteInfo byte_3    = type_byte;
		assert(byte_3.getKind() == Byte, "Byte should survive casting.");

		const TypeInfo type_bool = bool_1;
		const BoolInfo bool_3    = type_bool;
		assert(bool_3.getKind() == Bool, "Bool should survive casting.");

		const TypeInfo type_char = char_1;
		const CharInfo char_3    = type_char;
		assert(char_3.getKind() == Char, "Byte should survive casting.");
	}

	/**
	 * Test that there are signed and unsigned versions of each integral type
	 * of sizes from 8 bits to 128 bits, and that they are correctly cast.
	 */
	void simple_ints() {
		for (usize i = 0; i < 5; i++) {
			const auto int_1 = query::entryPoint<QueryIntegralType>({ 8U * (1 << i) });
			const auto int_2 = query::entryPoint<QueryIntegralType>({ 8U * (1 << i) });
			const auto int_u = query::entryPoint<QueryIntegralType>({ 8U * (1 << i), false });
			assert(int_1.getKind() == Integral, "Int should have kind Integral.");

			assert(int_1.getSize() == 8 * (1 << i), "Size of Int should be as constructed.");
			assert(int_1 == int_2, "Ints of the same size and signedness should be the same.");
			assert(
				int_1 != int_u,
				"Ints of the same size but different signedness should be the different."
			);

			TypeInfo     type_int = int_1;
			IntegralInfo int_3    = type_int;
			assert(int_3.getKind() == Integral, "Int should survive casting.");
		}

		assert(
			query::entryPoint<QueryIntegralType>({ 8 })
				!= query::entryPoint<QueryIntegralType>({ 16 }),
			"Ints of different sizes should be different."
		);
	}

	/**
	 * Test that there are floating point types of sizes from 16 bits to 128 bits,
	 * and that they are correctly cast.
	 */
	void simple_floats() {
		for (const std::array<usize, 5> float_sizes = { 16, 32, 64, 80, 128 };
		     const usize                float_size: float_sizes) {
			auto float_1 = query::entryPoint<QueryFloatType>(float_size);
			auto float_2 = query::entryPoint<QueryFloatType>(float_size);

			assert(float_1.getSize() == float_size, "Size of Float should be as constructed.");
			assert(float_1 == float_2, "Floats of the same size should be the same.");
			assert(float_1.getKind() == Float, "Floats should have float kind.");

			TypeInfo  type_float = float_1;
			FloatInfo float_3    = type_float;
			assert(float_3.getKind() == Float, "Float should survive casting.");
		}

		assert(
			query::entryPoint<QueryFloatType>(32) != query::entryPoint<QueryFloatType>(64),
			"Floats of different sizes should be different."
		);
	}

	/**
	 * Test that Raw Pointer and Pointer types correctly cast
	 * between each other and retain informaiton as expected.
	 */
	void simple_pointer() {
		const auto raw_1 = query::entryPoint<QueryRawPointerType>({});
		assert(raw_1.getKind() == RawPointer, "Raw Pointer should have kind RawPointer.");
		const auto raw_2 = query::entryPoint<QueryRawPointerType>({});
		assert(raw_1 == raw_2, "There should be only Raw Pointer.");

		const TypeInfo       type_raw = raw_1;
		const RawPointerInfo raw_3    = type_raw;
		assert(raw_3.getKind() == RawPointer, "Raw Pointer should survive casting.");

		const auto ptr_1 = query::entryPoint<QueryPointerType>({ raw_1 });
		assert(ptr_1.getKind() == Pointer, "Pointer should have kind Pointer.");

		const RawPointerInfo raw_ptr = ptr_1;
		const PointerInfo    ptr_2   = raw_ptr;
		assert(
			ptr_2.getKind() == Pointer && ptr_2.getUnderlyingType() == raw_1,
			"Pointer should survive casting."
		);
	}

	/**
	 * Test that function pointers and objects are treated as different types
	 * and that they are correctly cast.
	 */
	void simple_function() {
		const auto int_16 = TypeDesc<>(query::entryPoint<QueryIntegralType>({ 16 }));
		const auto int_32 = TypeDesc<>(query::entryPoint<QueryIntegralType>({ 32 }));

		const auto fun_1 = FunctionInfo::create({ int_16, int_32 }, int_32);

		assert(
			fun_1.getParameterTypes() == std::vector({ int_16, int_32 }),
			"Parameter types should be as constructed."
		);
		assert(fun_1.getResultType() == int_32, "Result type should be as constructed.");
		assert(fun_1.isPure() == false, "Purity should be as constructed.");
		assert(fun_1.isFree() == false, "Freedom should be as constructed.");

		const TypeInfo     type_fun = fun_1;
		const FunctionInfo fun2     = type_fun;
		assert(fun2.getKind() == Function, "Function should survive casting.");

		const auto fun_identical = FunctionInfo::create({ int_16, int_32 }, int_32);
		assert(fun_1 == fun_identical, "Function types constructed the same way should be equal.");
		const auto fun_different_input = FunctionInfo::create({ int_32, int_32 }, int_32);
		assert(
			fun_1 != fun_different_input, "Functions of different input types should be different."
		);
		const auto fun_different_output = FunctionInfo::create({ int_16, int_32 }, int_16);
		assert(
			fun_1 != fun_different_output,
			"Functions of different output types should be different."
		);
		const auto fun_different_flags
			= FunctionInfo::create({ int_16, int_32 }, int_32, true, true);
		assert(fun_1 != fun_different_flags, "Functions with different flags should be different.");

		const TypeInfo     type_fun_different_flags = fun_different_flags;
		const FunctionInfo fun_different_flags_2    = type_fun_different_flags;
		assert(
			fun_different_flags == fun_different_flags_2, "Function flags should survive casting."
		);
	}

	void simple_meta() {
		const auto meta   = query::entryPoint<QueryMetaType>({});
		const auto meta_2 = query::entryPoint<QueryMetaType>({});

		assert(meta == meta_2, "There shouldn't be multiple different 'type' types");
		assert(meta.getSize() == META_SIZE, "MetaType should have size META_SIZE");

		assert(meta.getKind() == Meta, "MetaType should have kind Meta");
	}

	void simple_type_desc() {
		const auto void_i = query::entryPoint<QueryVoidType>({});
		const auto int_i  = query::entryPoint<QueryIntegralType>({ 8 });

		const TypeDesc int_desc(int_i);

		try {
			TypeDesc<IntegralInfo>{ void_i };
			fail("Created IntegralDesc for Void type.");
		} catch (const base::LogicError&) {
			// expected
		}

		TypeDesc<>{ void_i };

		const TypeDesc really_int_desc = int_desc;

		assert(int_desc.getType().getKind() == Integral, "IntDesc type is not int");
		assert(
			really_int_desc.getType().getKind() == Integral,
			"IntDesc type after conversion to TypeDesc is not int"
		);
	}

	void simple_value_category() {
		const auto vc = ValueCategory(
			PrimaryCategory::Local,
			true,
			false,
			(MOVE | COPY | REINIT | USE | DESTROY),
			base::EmptyFlag
		);
		assert(
			vc.getCategory() == PrimaryCategory::Local,
			"ValueCategory constructor should initialize unchanged category value."
		);
		assert(
			vc.isMutable() == true,
			"ValueCategory constructor should initialize unchanged is_mutable value."
		);
		assert(
			vc.isPure() == false,
			"ValueCategory constructor should initialize unchanged is_pure value."
		);
		assert(
			vc.getAllowsSemantic() == (MOVE | COPY | REINIT | USE | DESTROY),
			"ValueCategory constructor should initialize unchanged allowsSemantic value."
		);
		assert(
			vc.getForceSemantic() == base::EmptyFlag,
			"ValueCategory constructor should initialize unchanged forceSemantic value."
		);

		const auto vc_1 = ValueCategory(
			PrimaryCategory::Local,
			true,
			false,
			(MOVE | COPY | REINIT | USE | DESTROY),
			base::EmptyFlag
		);
		assert(vc == vc_1, "Value categories constructed the same way should be equal.");

		const auto vc_2 = ValueCategory(
			PrimaryCategory::Local, true, false, (COPY | REINIT | USE), base::EmptyFlag
		);
		assert(
			vc_1.contains(vc_2),
			"Value category with full allows_semantic should contain same value category with "
			"subset of allowed semantics."
		);
	}

public:
	~SimpleTypeSystemTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/typesystem/tests/")
