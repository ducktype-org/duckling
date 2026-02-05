#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/expression_type.hpp>
#include <typesystem/higher/queries/implicit_coercibility.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/types.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <tester/tester.hpp>

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
		TESTER_ADD_TEST(simpleExpressionType);
		TESTER_ADD_TEST(simpleValueCategory);
		TESTER_ADD_TEST(simpleImplicitCoercibility);
	}

private:
	using enum Kind;

	/**
	 * Test that the specialized AbstractType to AbstractType dynamic cast works as intended.
	 * Also, test that assignment works.
	 */
	void trivialCastAndAssignment() {
		const AbstractType type_1{ query::entryPoint<QueryVoidType>({}) };
		AbstractType       type_2 = type_1;
		assertTrue(type_1 == type_2, "The trivial dynamic cast should not change any objects.");
		type_2 = query::entryPoint<QueryUnitType>({});
		assertTrue(type_1 != type_2, "Assignment on AbstractType should change the target object.");
	}

	/**
	 * Test that there is only one void and one unit type, and that they are correctly cast.
	 */
	void simpleVoidAndUnit() {
		const auto void_1 = query::entryPoint<QueryVoidType>({});
		const auto void_2 = query::entryPoint<QueryVoidType>({});
		assertTrue(void_1 == void_2, "There should only be one Void type.");
		assertTrue(void_1.getKind() == Void, "Void type should have kind Void.");
		assertTrue(void_1.hasNoOpDestructor(), "Void should have no op destructor.");

		const auto unit_1 = query::entryPoint<QueryUnitType>({});
		const auto unit_2 = query::entryPoint<QueryUnitType>({});
		assertTrue(unit_1 == unit_2, "There should only be one Unit type.");
		assertTrue(unit_1.getKind() == Unit, "Unit type should have kind Unit.");
		assertTrue(unit_1.hasNoOpDestructor(), "Unit should have no op destructor.");

		assertTrue(void_1 != unit_1, "Void and Unit should be different types.");

		const AbstractType     type_void = void_1;
		const VoidAbstractType void_3    = type_void;
		assertTrue(void_3.getKind() == Void, "Void should survive casting.");

		const AbstractType     type_unit = unit_1;
		const UnitAbstractType unit_3    = type_unit;
		assertTrue(unit_3.getKind() == Unit, "Unit should survive casting.");
	}

	/**
	 * Test that there are three unique byte-sized types, and that they are correctly cast.
	 */
	void simpleByteSized() {
		const auto byte_1 = query::entryPoint<QueryByteType>({});
		assertTrue(byte_1.getKind() == Byte, "Byte type should have kind Byte.");
		assertTrue(byte_1.hasNoOpDestructor(), "Byte should have no op destructor.");
		const auto bool_1 = query::entryPoint<QueryBoolType>({});
		assertTrue(bool_1.getKind() == Bool, "Bool type should have kind Bool.");
		assertTrue(bool_1.hasNoOpDestructor(), "Bool should have no op destructor.");
		const auto char_1 = query::entryPoint<QueryCharType>({});
		assertTrue(char_1.getKind() == Char, "Char type should have kind Char.");
		assertTrue(char_1.hasNoOpDestructor(), "Char should have no op destructor.");

		assertTrue(
			byte_1 != bool_1 && bool_1 != char_1 && char_1 != byte_1,
			"All byte-sized types should be different."
		);

		const auto byte_2 = query::entryPoint<QueryByteType>({});
		const auto bool_2 = query::entryPoint<QueryBoolType>({});
		const auto char_2 = query::entryPoint<QueryCharType>({});

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
	}

	/**
	 * Test that there are signed and unsigned versions of each integral type
	 * of sizes from 8 bits to 128 bits, and that they are correctly cast.
	 */
	void simpleInts() {
		using enum IntegralAbstractType::Signedness;

		for (usize i = 0; i < 5; i++) {
			const auto int_1 = query::entryPoint<QueryIntegralType>({ 8U * (1 << i) });
			const auto int_2 = query::entryPoint<QueryIntegralType>({ 8U * (1 << i) });
			const auto int_u = query::entryPoint<QueryIntegralType>({ 8U * (1 << i), Unsigned });
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
			query::entryPoint<QueryIntegralType>({ 8 })
				!= query::entryPoint<QueryIntegralType>({ 16 }),
			"Ints of different sizes should be different."
		);
	}

	/**
	 * Test that there are floating point types of sizes from 16 bits to 128 bits,
	 * and that they are correctly cast.
	 */
	void simpleFloats() {
		for (const std::array<usize, 5> float_sizes = { 16, 32, 64, 80, 128 };
		     const usize                float_size: float_sizes) {
			auto float_1 = query::entryPoint<QueryFloatType>({ float_size });
			auto float_2 = query::entryPoint<QueryFloatType>({ float_size });

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
			query::entryPoint<QueryFloatType>({ 32 }) != query::entryPoint<QueryFloatType>({ 64 }),
			"Floats of different sizes should be different."
		);
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
		const auto raw_1 = query::entryPoint<QueryRawPointerType>({ false });
		assertTrue(raw_1.getKind() == RawPointer, "Raw Pointer should have kind RawPointer.");
		const auto raw_2 = query::entryPoint<QueryRawPointerType>({ true });
		assertTrue(
			raw_2.getKind() == RawPointer, "Mutable Raw Pointer should have kind RawPointer."
		);
		assertTrue(raw_1 != raw_2, "Immutable and mutable Raw Pointers should be different.");
		const auto raw_3 = query::entryPoint<QueryRawPointerType>({ false });
		assertTrue(raw_1 == raw_3, "There should be only one immutable Raw Pointer.");
		const auto raw_4 = query::entryPoint<QueryRawPointerType>({ true });
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
	}

	/**
	 * Test that there is only one string type, and that it is correctly cast.
	 */
	void simpleString() {
		const auto str_1 = query::entryPoint<QueryStringType>({});
		const auto str_2 = query::entryPoint<QueryStringType>({});

		assertTrue(str_1 == str_2, "There should only be one String type.");

		assertTrue(str_1.getKind() == String, "String type should have kind String.");

		const AbstractType       type_str = str_1;
		const StringAbstractType str_3    = type_str;
		assertTrue(str_3.getKind() == String, "String should survive casting.");

		assertFalse(str_1.hasNoOpDestructor(), "String should not have no op destructor.");
	}

	/**
	 * Test that dynamic array with different elements are different types
	 * and that they are correctly cast.
	 */
	void simpleDynamicArray() {
		const auto int_16 = query::entryPoint<QueryIntegralType>({ 16 });
		const auto int_32 = query::entryPoint<QueryIntegralType>({ 32 });

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
	}

	/**
	 * Test that static arrays with different properties are treated as different types
	 * and that they are correctly cast and handled.
	 */
	void simpleStaticArray() {
		const auto int_16 = query::entryPoint<QueryIntegralType>({ 16 });
		const auto int_32 = query::entryPoint<QueryIntegralType>({ 32 });

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

		const auto str_type = query::entryPoint<QueryStringType>({});
		const auto arr_str  = query::entryPoint<QueryStaticArrayType>({ st(str_type), 5 });
		assertFalse(
			arr_str.hasNoOpDestructor(), "StaticArray of Strings should not have a no-op destructor."
		);

		const auto ptr_to_int16 = query::entryPoint<QueryPointerType>({ st(int_16) });
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ arr_1, ptr_to_int16 }),
			"Static array should be coercible to a pointer of its element type."
		);

		const auto ptr_to_int32 = query::entryPoint<QueryPointerType>({ st(int_32) });
		assertFalse(
			query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ arr_1, ptr_to_int32 }),
			"Static array should not be coercible to a pointer of a different type."
		);
	}

	/**
	 * Test that tuples with different components are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleTuple() {
		const auto int_16 = query::entryPoint<QueryIntegralType>({ 16 });
		const auto int_32 = query::entryPoint<QueryIntegralType>({ 32 });

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

		const auto str   = query::entryPoint<QueryStringType>({});
		const auto tup_6 = query::entryPoint<QueryTupleType>({ { st(int_16), st(str) } });
		assertFalse(
			tup_6.hasNoOpDestructor(), "Tuple with String should not have no op destructor."
		);
	}

	/**
	 * Test that variants with different components are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleVariant() {
		const auto int_16 = query::entryPoint<QueryIntegralType>({ 16 });
		const auto int_32 = query::entryPoint<QueryIntegralType>({ 32 });

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

		const auto str   = query::entryPoint<QueryStringType>({});
		const auto var_5 = query::entryPoint<QueryVariantType>({ { st(int_16), st(str) } });
		assertFalse(
			var_5.hasNoOpDestructor(), "Variant with String should not have no op destructor."
		);
	}

	/**
	 * Test that function pointers and objects are treated as different types
	 * and that they are correctly cast.
	 */
	void simpleFunction() {
		const auto int_16 = query::entryPoint<QueryIntegralType>({ 16 });
		const auto int_32 = query::entryPoint<QueryIntegralType>({ 32 });

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
	}

	void simpleLanguageElements() {
		const auto nspace   = query::entryPoint<QueryNamespaceType>({});
		const auto nspace_2 = query::entryPoint<QueryNamespaceType>({});

		assertTrue(nspace == nspace_2, "There shouldn't be multiple different Namespace types.");

		assertTrue(nspace.getKind() == Namespace, "NamespaceType should have kind Meta.");
		assertTrue(nspace.hasNoOpDestructor(), "NamespaceType should have no op destructor.");

		const AbstractType          nspace_type = nspace;
		const NamespaceAbstractType nspace_3    = nspace_type;
		assertTrue(nspace_3.getKind() == Namespace, "NamespaceType should survive casting.");

		const auto module   = query::entryPoint<QueryModuleType>({});
		const auto module_2 = query::entryPoint<QueryModuleType>({});

		assertTrue(module == module_2, "There shouldn't be multiple different Module types.");

		assertTrue(module.getKind() == Module, "ModuleType should have kind Meta.");
		assertFalse(module.hasNoOpDestructor(), "ModuleType should not have no op destructor.");

		const AbstractType       module_type = module;
		const ModuleAbstractType module_3    = module_type;
		assertTrue(module_3.getKind() == Module, "ModuleType should survive casting.");
	}

	void simpleMeta() {
		const auto meta   = query::entryPoint<QueryMetaType>({});
		const auto meta_2 = query::entryPoint<QueryMetaType>({});

		assertTrue(meta == meta_2, "There shouldn't be multiple different 'type' types.");

		assertTrue(meta.getKind() == Meta, "MetaType should have kind Meta.");
		assertTrue(meta.hasNoOpDestructor(), "MetaType should have no op destructor.");

		const AbstractType     meta_type = meta;
		const MetaAbstractType met_3     = meta_type;
		assertTrue(met_3.getKind() == Meta, "MetaType should survive casting.");
	}

	void simpleExpressionType() {
		const auto void_i = query::entryPoint<QueryVoidType>({});
		const auto int_i  = query::entryPoint<QueryIntegralType>({ 8 });

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
		const auto int_2 = query::entryPoint<QueryIntegralType>({ 8U * (1 << 2) });
		const auto int_3 = query::entryPoint<QueryIntegralType>({ 8U * (1 << 3) });
		assertTrue(
			query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_2, int_3 }),
			"Smaller int should be coercible into a bigger one."
		);

		assertTrue(
			!query::entryPoint<QueryImplicitCoercibilityOnAbstractType>({ int_3, int_2 }),
			"Bigger int should not be coercible into a smaller one."
		);

		const auto void_type = query::entryPoint<QueryVoidType>({});
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

TESTER_COMMON_MAIN("/src/compiler/core/typesystem/tests/")
