#include <abi/type_system/type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>
#include <tsl/c_abi_target.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <variant>

using namespace compiler::tsh;
using namespace compiler::tsl;
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

	const CAbiConversionResult& queryConv(query::Context& ctx, SymbolType<> st) {
		auto qr = ctx.query<QueryCAbiTypeOf>(st);
		CORE_ASSERT(!qr->hasFailed(), "QueryCAbiTypeOf returned Failed unexpectedly");
		return qr->valueOrThrow();
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
		TESTER_ADD_TEST(cPointerTest);
		TESTER_ADD_TEST(typedPointerRejectedTest);
		TESTER_ADD_TEST(manyPointerRejectedTest);
		TESTER_ADD_TEST(staticArrayOfIntTest);
		TESTER_ADD_TEST(staticArrayZeroCountRejectedTest);
		TESTER_ADD_TEST(staticArrayOfRefRejectedTest);
		TESTER_ADD_TEST(refRejectedTest);
		TESTER_ADD_TEST(boxRejectedTest);
		TESTER_ADD_TEST(floatWidthsTest);
		TESTER_ADD_TEST(floatExtendedX87Test);
		TESTER_ADD_TEST(boolAcceptedTest);
		TESTER_ADD_TEST(charAcceptedTest);
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

	void expectFloat(const ats::AbiType& abi_type, usize width) {
		ASSERT_TRUE(std::holds_alternative<ats::FloatType>(abi_type.value));
		const auto& f = std::get<ats::FloatType>(abi_type.value);
		assertTrue(usize(f.width_bits) == width, "width mismatch");
	}

	void expectPointer(const ats::AbiType& abi_type) {
		assertTrue(std::holds_alternative<ats::PointerType>(abi_type.value), "expected PointerType");
	}

	void integerWidthsTest() {
		withContextDo([&](query::Context& ctx) -> void {
			for (usize width: { usize(8), usize(16), usize(32), usize(64) }) {
				const auto t
					= getIntegralType(ctx, width, IntegralAbstractType::Signedness::Signed);
				const auto& r = queryConv(ctx, directOf(t));
				ASSERT_TRUE(r.abi_type.has_value());
				expectInt(*r.abi_type, width, true);
			}
		});
	}

	void integerSignednessTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto u32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Unsigned);
			const auto& r = queryConv(ctx, directOf(u32_type));
			ASSERT_TRUE(r.abi_type.has_value());
			expectInt(*r.abi_type, 32, false);
		});
	}

	void integerUnsupportedWidthTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i128_type
				= getIntegralType(ctx, 128, IntegralAbstractType::Signedness::Signed);
			const auto& r = queryConv(ctx, directOf(i128_type));
			assertFalse(r.abi_type.has_value(), "i128 should be rejected");
			assertTrue(r.reason.find("128") != std::string::npos, "reason should mention width");
		});
	}

	void byteTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getByteType()));
			ASSERT_TRUE(r.abi_type.has_value());
			expectInt(*r.abi_type, 8, false);
		});
	}

	void rawPointerTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getRawPointerType(false)));
			assertFalse(r.abi_type.has_value(), "raw pointer should be rejected");
			const bool has_c_compat = r.reason.find("c-compatible") != std::string::npos;
			const bool has_cptr     = r.reason.find("cptr") != std::string::npos;
			assertTrue(has_c_compat || has_cptr, "reason should mention C-compatibility or cptr");
		});
	}

	void cPointerTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const CPointerAbstractType c_pointer
				= ctx.query<QueryCPointerType>({ directOf(i32_type) });
			const auto& r = queryConv(ctx, directOf(c_pointer));
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
			const auto& r = queryConv(ctx, directOf(typed_pointer));
			assertFalse(r.abi_type.has_value(), "typed pointer should be rejected");
		});
	}

	void manyPointerRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const ManyPointerAbstractType many_pointer
				= ctx.query<QueryManyPointerType>({ directOf(i32_type) });
			const auto& r = queryConv(ctx, directOf(many_pointer));
			assertFalse(r.abi_type.has_value(), "many-pointer should be rejected");
		});
	}

	void staticArrayOfIntTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ directOf(i32_type), 4 });
			const auto& r = queryConv(ctx, directOf(arr_type));
			ASSERT_TRUE(r.abi_type.has_value());
			ASSERT_TRUE(std::holds_alternative<ats::OpaqueType>(r.abi_type->value));
			const auto& o = std::get<ats::OpaqueType>(r.abi_type->value);
			assertTrue(usize(o.size) == 16, "array of 4 i32 should be 16 bytes");
			assertTrue(usize(o.alignment) == 4, "array of i32 should have align 4");
		});
	}

	void staticArrayZeroCountRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ directOf(i32_type), 0 });
			const auto& r = queryConv(ctx, directOf(arr_type));
			assertFalse(r.abi_type.has_value(), "zero-length array should be rejected");
		});
	}

	void staticArrayOfRefRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ refOf(i32_type), 4 });
			const auto& r = queryConv(ctx, directOf(arr_type));
			assertFalse(r.abi_type.has_value(), "array of ref should be rejected");
		});
	}

	void refRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i64_type
				= getIntegralType(ctx, 64, IntegralAbstractType::Signedness::Signed);
			const auto& r = queryConv(ctx, refOf(i64_type));
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
			const auto& r = queryConv(ctx, boxOf(i64_type));
			assertFalse(r.abi_type.has_value(), "box should be rejected");
		});
	}

	void floatWidthsTest() {
		// IEEE binary formats are C-compatible on every target.
		withContextDo([&](query::Context& ctx) -> void {
			for (usize width: { usize(16), usize(32), usize(64), usize(128) }) {
				const auto  t = getFloatType(ctx, width);
				const auto& r = queryConv(ctx, directOf(t));
				ASSERT_TRUE(r.abi_type.has_value());
				expectFloat(*r.abi_type, width);
			}
		});
	}

	void floatExtendedX87Test() {
		// f80 (x87 long double) is accepted only on targets with an x87 unit;
		// elsewhere it is rejected with a clean diagnostic.
		withContextDo([&](query::Context& ctx) -> void {
			const auto  f80_type = getFloatType(ctx, 80);
			const auto& r        = queryConv(ctx, directOf(f80_type));
			if (compilerTargetABI().data_layout.float_layouts.atMaybe(u8(80)).has_value()) {
				ASSERT_TRUE(r.abi_type.has_value());
				expectFloat(*r.abi_type, 80);
			} else {
				assertFalse(r.abi_type.has_value(), "f80 should be rejected without an x87 unit");
			}
		});
	}

	void boolAcceptedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getBoolType()));
			ASSERT_TRUE(r.abi_type.has_value());
			assertTrue(
				std::holds_alternative<ats::BoolType>(r.abi_type->value), "expected BoolType"
			);
		});
	}

	void charAcceptedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getCharType()));
			ASSERT_TRUE(r.abi_type.has_value());
			assertTrue(
				std::holds_alternative<ats::CharType>(r.abi_type->value), "expected CharType"
			);
		});
	}

	void stringRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getStringType()));
			assertFalse(r.abi_type.has_value(), "string should be rejected");
		});
	}

	void unitRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& r = queryConv(ctx, directOf(getUnitType()));
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
			const auto& r = queryConv(ctx, directOf(dyn));
			assertFalse(r.abi_type.has_value(), "dynamic array should be rejected");
		});
	}

	void tupleRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
			const TupleAbstractType tup
				= ctx.query<QueryTupleType>({ { directOf(i32_type), directOf(i32_type) } });
			const auto& r = queryConv(ctx, directOf(tup));
			assertFalse(r.abi_type.has_value(), "tuple should be rejected");
		});
	}

	void functionRejectedTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const FunctionAbstractType fun
				= ctx.query<QueryFunctionType>({ {}, directOf(getUnitType()) });
			const auto& r = queryConv(ctx, directOf(fun));
			assertFalse(r.abi_type.has_value(), "function should be rejected");
		});
	}

public:
	~CAbiConverterTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/tsl/tests/")
