#include <abi/type_system/type.hpp>

#include <tester/tester.hpp>

#include <variant>

class AbiTypeSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AbiTypeSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(intTypeConstructionTest);
		TESTER_ADD_TEST(pointerTypeIsEmptyTest);
		TESTER_ADD_TEST(arrayAndStructHelpersTest);
		TESTER_ADD_TEST(nestedStructTest);
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

		std::vector<Field> fs;
		fs.push_back(field(std::string("x"), intType(u8(64), true)));
		fs.push_back(field(base::Optional<std::string>{}, pointerType()));
		AbiType s = structType(std::move(fs));

		assertTrue(std::holds_alternative<StructType>(s.value), "should be StructType");
		const auto& sv = std::get<StructType>(s.value);
		assertTrue(sv.fields.size() == 2, "should have two fields");
		assertTrue(sv.fields[0].name.has_value(), "first field has a name");
		assertTrue(!sv.fields[1].name.has_value(), "second field is anonymous");
	}

	void nestedStructTest() {
		using namespace abi::type_system;

		std::vector<Field> inner_fields;
		inner_fields.push_back(field(std::string("a"), intType(u8(8), true)));
		inner_fields.push_back(field(std::string("b"), intType(u8(64), true)));

		std::vector<Field> outer_fields;
		outer_fields.push_back(field(std::string("x"), intType(u8(64), true)));
		outer_fields.push_back(field(std::string("y"), structType(std::move(inner_fields))));

		AbiType     outer = structType(std::move(outer_fields));
		const auto& sv    = std::get<StructType>(outer.value);
		assertTrue(sv.fields.size() == 2, "outer should have two fields");
		const auto& inner = std::get<StructType>(sv.fields[1].type->value);
		assertTrue(inner.fields.size() == 2, "inner should have two fields");
	}
};

TESTER_COMMON_MAIN("/src/common/abi/type_system/tests/");
