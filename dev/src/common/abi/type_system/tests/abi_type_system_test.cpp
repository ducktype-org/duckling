#include <abi/type_system/type.hpp>

#include <tester/tester.hpp>

#include <variant>

class AbiTypeSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AbiTypeSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(intTypeConstructionTest);
		TESTER_ADD_TEST(floatTypeConstructionTest);
		TESTER_ADD_TEST(boolAndCharConstructionTest);
		TESTER_ADD_TEST(pointerTypeIsEmptyTest);
		TESTER_ADD_TEST(arrayAndStructHelpersTest);
		TESTER_ADD_TEST(nestedStructTest);
		TESTER_ADD_TEST(opaqueTypeConstructionTest);
	}

private:
	void intTypeConstructionTest() {
		using namespace abi::type_system;

		AbiType i32_t = intType(u8(32), true);
		assertTrue(std::holds_alternative<IntType>(i32_t.value), "i32 should be IntType");
		const auto& i = std::get<IntType>(i32_t.value);
		assertTrue(usize(i.width_bits) == 32, "width should be 32");
		assertTrue(i.is_signed, "i32 should be signed");

		AbiType u8_t = intType(u8(8), false);
		assertTrue(usize(std::get<IntType>(u8_t.value).width_bits) == 8, "u8 width should be 8");
		assertTrue(!std::get<IntType>(u8_t.value).is_signed, "u8 should be unsigned");
	}

	void floatTypeConstructionTest() {
		using namespace abi::type_system;

		AbiType f32_t = floatType(u8(32));
		assertTrue(std::holds_alternative<FloatType>(f32_t.value), "f32 should be FloatType");
		assertTrue(usize(std::get<FloatType>(f32_t.value).width_bits) == 32, "width should be 32");

		AbiType f64_t = floatType(u8(64));
		assertTrue(usize(std::get<FloatType>(f64_t.value).width_bits) == 64, "width should be 64");

		AbiType cloned = cloneAbiType(f64_t);
		assertTrue(std::holds_alternative<FloatType>(cloned.value), "clone preserves FloatType");
		assertTrue(
			usize(std::get<FloatType>(cloned.value).width_bits) == 64, "clone preserves width"
		);
	}

	void boolAndCharConstructionTest() {
		using namespace abi::type_system;

		AbiType b = boolType();
		assertTrue(std::holds_alternative<BoolType>(b.value), "should be BoolType");
		AbiType c = charType();
		assertTrue(std::holds_alternative<CharType>(c.value), "should be CharType");

		AbiType cloned = cloneAbiType(b);
		assertTrue(std::holds_alternative<BoolType>(cloned.value), "clone preserves BoolType");
	}

	void pointerTypeIsEmptyTest() {
		using namespace abi::type_system;

		AbiType p = pointerType();
		assertTrue(std::holds_alternative<PointerType>(p.value), "should be PointerType");
		static_assert(sizeof(PointerType) == 1, "PointerType is expected to be an empty marker");
	}

	void arrayAndStructHelpersTest() {
		using namespace abi::type_system;

		AbiType arr = arrayType(intType(u8(32), true), 4);
		assertTrue(std::holds_alternative<ArrayType>(arr.value), "should be ArrayType");
		const auto& a = std::get<ArrayType>(arr.value);
		assertTrue(a.count == 4, "count should be 4");
		assertTrue(std::holds_alternative<IntType>(a.element->value), "element should be IntType");

		std::vector<AbiTypePtr> fs;
		fs.push_back(makeAbiType(intType(u8(64), true)));
		fs.push_back(makeAbiType(pointerType()));
		AbiType s = structType(std::move(fs));

		assertTrue(std::holds_alternative<StructType>(s.value), "should be StructType");
		const auto& sv = std::get<StructType>(s.value);
		assertTrue(sv.fields.size() == 2, "should have two fields");
		assertTrue(std::holds_alternative<IntType>(sv.fields[0]->value), "first field is IntType");
		assertTrue(
			std::holds_alternative<PointerType>(sv.fields[1]->value), "second field is PointerType"
		);
	}

	void nestedStructTest() {
		using namespace abi::type_system;

		std::vector<AbiTypePtr> inner_fields;
		inner_fields.push_back(makeAbiType(intType(u8(8), true)));
		inner_fields.push_back(makeAbiType(intType(u8(64), true)));

		std::vector<AbiTypePtr> outer_fields;
		outer_fields.push_back(makeAbiType(intType(u8(64), true)));
		outer_fields.push_back(makeAbiType(structType(std::move(inner_fields))));

		AbiType     outer = structType(std::move(outer_fields));
		const auto& sv    = std::get<StructType>(outer.value);
		assertTrue(sv.fields.size() == 2, "outer should have two fields");
		const auto& inner = std::get<StructType>(sv.fields[1]->value);
		assertTrue(inner.fields.size() == 2, "inner should have two fields");
	}

	void opaqueTypeConstructionTest() {
		using namespace abi::type_system;

		AbiType o = opaqueType(Bytes(16), Bytes(8));
		assertTrue(std::holds_alternative<OpaqueType>(o.value), "should be OpaqueType");
		const auto& op = std::get<OpaqueType>(o.value);
		assertTrue(usize(op.size) == 16, "size should be 16");
		assertTrue(usize(op.alignment) == 8, "alignment should be 8");
	}
};

TESTER_COMMON_MAIN("/src/common/abi/type_system/tests/");
