#include <abi/type_system/type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <variant>

using namespace compiler::tsh;
using query::utils::withContextDo;
namespace ats = abi::type_system;

namespace {

	SymbolType<> directOf(AbstractType abstract_type) {
		return SymbolType{ abstract_type, ReferenceKind::Direct, Mutability::Immutable };
	}

	SymbolType<> refOf(AbstractType abstract_type) {
		return SymbolType{ abstract_type, ReferenceKind::Ref, Mutability::Immutable };
	}

	SymbolType<> boxOf(AbstractType abstract_type) {
		return SymbolType{ abstract_type, ReferenceKind::Box, Mutability::Immutable };
	}

}

class CAbiConverterTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CAbiConverterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(integerWidthsTest);
		TESTER_ADD_TEST(integerSignednessTest);
		TESTER_ADD_TEST(integerUnsupportedWidthTest);
		TESTER_ADD_TEST(byteTest);
		TESTER_ADD_TEST(rawPointerTest);
		TESTER_ADD_TEST(typedPointerRejectedTest);
		TESTER_ADD_TEST(staticArrayOfIntTest);
		TESTER_ADD_TEST(staticArrayZeroCountRejectedTest);
		TESTER_ADD_TEST(staticArrayOfRefRejectedTest);
		TESTER_ADD_TEST(refRejectedTest);
		TESTER_ADD_TEST(boxRejectedTest);
		TESTER_ADD_TEST(floatRejectedTest);
		TESTER_ADD_TEST(boolRejectedTest);
		TESTER_ADD_TEST(charRejectedTest);
		TESTER_ADD_TEST(stringRejectedTest);
		TESTER_ADD_TEST(unitRejectedTest);
		TESTER_ADD_TEST(dynamicArrayRejectedTest);
		TESTER_ADD_TEST(tupleRejectedTest);
		TESTER_ADD_TEST(functionRejectedTest);
	}

private:
	void expectInt(const ats::AbiType& abi_type, usize width, bool is_signed) {
		ASSERT_TRUE(std::holds_alternative<ats::IntType>(abi_type.value));
		const auto& i = std::get<ats::IntType>(abi_type.value);
		assertTrue(usize(i.width_bits) == width, "width mismatch");
		assertTrue(i.is_signed == is_signed, "signedness mismatch");
	}

	void expectPointer(const ats::AbiType& abi_type) {
		assertTrue(std::holds_alternative<ats::PointerType>(abi_type.value), "expected PointerType");
	}

	void integerWidthsTest() {
		withContextDo([&](query::Context& ctx) -> void {
			for (usize width: { usize(8), usize(16), usize(32), usize(64) }) {
				const auto t
					= getIntegralType(ctx, width, IntegralAbstractType::Signedness::Signed);
				auto r = compiler::tsl::tryConvertToCAbiType(directOf(t), ctx);
				ASSERT_TRUE(r.abi_type.has_value());
				expectInt(*r.abi_type, width, true);
			}
		});
	}

	void integerSignednessTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto u32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Unsigned);
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(u32_type), ctx);
			ASSERT_TRUE(r.abi_type.has_value());
			expectInt(*r.abi_type, 32, false);
		});
	}

	void integerUnsupportedWidthTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i128_type
				= getIntegralType(ctx, 128, IntegralAbstractType::Signedness::Signed);
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(i128_type), ctx);
			assertFalse(r.abi_type.has_value(), "i128 should be rejected");
			assertTrue(r.reason.find("128") != std::string::npos, "reason should mention width");
		});
	}

	void byteTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getByteType()), ctx);
			ASSERT_TRUE(r.abi_type.has_value());
			expectInt(*r.abi_type, 8, false);
		});
	}

	void rawPointerTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getRawPointerType(false)), ctx);
			ASSERT_TRUE(r.abi_type.has_value());
			expectPointer(*r.abi_type);
		});
	}

	void typedPointerRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const PointerAbstractType typed_pointer
				= ctx.query<QueryPointerType>({ directOf(i32_type) });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(typed_pointer), ctx);
			assertFalse(r.abi_type.has_value(), "typed pointer should be rejected");
		});
	}

	void staticArrayOfIntTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ directOf(i32_type), 4 });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(arr_type), ctx);
			ASSERT_TRUE(r.abi_type.has_value());
			ASSERT_TRUE(std::holds_alternative<ats::ArrayType>(r.abi_type->value));
			const auto& a = std::get<ats::ArrayType>(r.abi_type->value);
			assertTrue(a.count == 4, "count mismatch");
			expectInt(*a.element, 32, true);
		});
	}

	void staticArrayZeroCountRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ directOf(i32_type), 0 });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(arr_type), ctx);
			assertFalse(r.abi_type.has_value(), "zero-length array should be rejected");
		});
	}

	void staticArrayOfRefRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ refOf(i32_type), 4 });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(arr_type), ctx);
			assertFalse(r.abi_type.has_value(), "array of ref should be rejected");
		});
	}

	void refRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i64_type
				= getIntegralType(ctx, 64, IntegralAbstractType::Signedness::Signed);
			auto r = compiler::tsl::tryConvertToCAbiType(refOf(i64_type), ctx);
			assertFalse(r.abi_type.has_value(), "ref should be rejected");
			assertTrue(
				r.reason.find("reference") != std::string::npos, "reason should mention reference"
			);
		});
	}

	void boxRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i64_type
				= getIntegralType(ctx, 64, IntegralAbstractType::Signedness::Signed);
			auto r = compiler::tsl::tryConvertToCAbiType(boxOf(i64_type), ctx);
			assertFalse(r.abi_type.has_value(), "box should be rejected");
		});
	}

	void floatRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto f32_type = getFloatType(ctx, 32);
			auto       r        = compiler::tsl::tryConvertToCAbiType(directOf(f32_type), ctx);
			assertFalse(r.abi_type.has_value(), "float should be rejected");
		});
	}

	void boolRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getBoolType()), ctx);
			assertFalse(r.abi_type.has_value(), "bool should be rejected");
		});
	}

	void charRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getCharType()), ctx);
			assertFalse(r.abi_type.has_value(), "char should be rejected");
		});
	}

	void stringRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getStringType()), ctx);
			assertFalse(r.abi_type.has_value(), "string should be rejected");
		});
	}

	void unitRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(getUnitType()), ctx);
			assertFalse(r.abi_type.has_value(), "unit should be rejected");
			assertTrue(
				r.reason.find("zero") != std::string::npos, "reason should mention zero size"
			);
		});
	}

	void dynamicArrayRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const DynamicArrayAbstractType dyn
				= ctx.query<QueryDynamicArrayType>(directOf(i32_type));
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(dyn), ctx);
			assertFalse(r.abi_type.has_value(), "dynamic array should be rejected");
		});
	}

	void tupleRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const TupleAbstractType tup
				= ctx.query<QueryTupleType>({ { directOf(i32_type), directOf(i32_type) } });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(tup), ctx);
			assertFalse(r.abi_type.has_value(), "tuple should be rejected");
		});
	}

	void functionRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const FunctionAbstractType fun
				= ctx.query<QueryFunctionType>({ {}, directOf(getUnitType()) });
			auto r = compiler::tsl::tryConvertToCAbiType(directOf(fun), ctx);
			assertFalse(r.abi_type.has_value(), "function should be rejected");
		});
	}

public:
	~CAbiConverterTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/tsl/tests/")
