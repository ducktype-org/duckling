#include <abi/type_system/type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>
#include <tsl/c_abi_target.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

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

	IntegralAbstractType i32Of(query::Context& ctx) {
		return getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Signed);
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
		TESTER_ADD_TEST(integersTest);
		TESTER_ADD_TEST(floatsTest);
		TESTER_ADD_TEST(boolCharPointerTest);
		TESTER_ADD_TEST(arraysTest);
		TESTER_ADD_TEST(rejectedTypesTest);
	}

private:
	void expectInt(const CAbiConversionResult& r, usize width, bool is_signed) {
		ASSERT_TRUE(r.has_value());
		ASSERT_TRUE(base::holds<ats::IntType>(r->value));
		const auto& i = std::get<ats::IntType>(r->value);
		assertTrue(usize(i.width_bits) == width, "width mismatch");
		assertTrue(i.is_signed == is_signed, "signedness mismatch");
	}

	void expectFloat(const CAbiConversionResult& r, usize width) {
		ASSERT_TRUE(r.has_value());
		ASSERT_TRUE(base::holds<ats::FloatType>(r->value));
		assertTrue(usize(std::get<ats::FloatType>(r->value).width_bits) == width, "width mismatch");
	}

	void expectRejected(query::Context& ctx, SymbolType<> st, const std::string& what) {
		const auto& r = queryConv(ctx, st);
		assertFalse(r.has_value(), what + " should be rejected");
	}

	void integersTest() {
		withContextDo([&](query::Context& ctx) -> void {
			for (usize width: { usize(8), usize(16), usize(32), usize(64) }) {
				const auto t
					= getIntegralType(ctx, width, IntegralAbstractType::Signedness::Signed);
				expectInt(queryConv(ctx, directOf(t)), width, true);
			}
			const auto u32_type
				= getIntegralType(ctx, 32, IntegralAbstractType::Signedness::Unsigned);
			expectInt(queryConv(ctx, directOf(u32_type)), 32, false);

			// `byte` converts to an unsigned 8-bit integer.
			expectInt(queryConv(ctx, directOf(getByteType())), 8, false);
		});
	}

	void floatsTest() {
		withContextDo([&](query::Context& ctx) -> void {
			// IEEE binary formats are C-compatible on every target.
			for (usize width: { usize(16), usize(32), usize(64), usize(128) }) {
				const auto t = getFloatType(ctx, width);
				expectFloat(queryConv(ctx, directOf(t)), width);
			}

			// f80 (x87 long double) is accepted only on x86_64 (x87 unit);
			// aarch64 has no such format and must reject it.
			const auto  f80_type = getFloatType(ctx, 80);
			const auto& r        = queryConv(ctx, directOf(f80_type));
			if (compilerTargetABI().data_layout.float_layouts.contains(u8(80)))
				expectFloat(r, 80);
			else
				assertFalse(r.has_value(), "f80 should be rejected without an x87 unit");
		});
	}

	void boolCharPointerTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto& bool_conv = queryConv(ctx, directOf(getBoolType()));
			ASSERT_TRUE(bool_conv.has_value());
			assertTrue(base::holds<ats::BoolType>(bool_conv->value), "expected BoolType");

			const auto& char_conv = queryConv(ctx, directOf(getCharType()));
			ASSERT_TRUE(char_conv.has_value());
			assertTrue(base::holds<ats::CharType>(char_conv->value), "expected CharType");

			const CPointerAbstractType c_pointer
				= ctx.query<QueryCPointerType>({ directOf(i32Of(ctx)) });
			const auto& ptr_conv = queryConv(ctx, directOf(c_pointer));
			ASSERT_TRUE(ptr_conv.has_value());
			assertTrue(base::holds<ats::PointerType>(ptr_conv->value), "expected PointerType");
		});
	}

	void arraysTest() {
		withContextDo([&](query::Context& ctx) -> void {
			// Accepted: a static array converts to an ArrayType whose element
			// borrows the element type's cached conversion (no clone).
			const StaticArrayAbstractType arr_type
				= ctx.query<QueryStaticArrayType>({ directOf(i32Of(ctx)), 4 });
			const auto& r = queryConv(ctx, directOf(arr_type));
			ASSERT_TRUE(r.has_value());
			ASSERT_TRUE(base::holds<ats::ArrayType>(r->value));
			const auto& a = std::get<ats::ArrayType>(r->value);
			assertTrue(a.count == 4, "array should have 4 elements");
			ASSERT_TRUE(base::holds<ats::IntType>(a.element->value));
			assertTrue(
				usize(std::get<ats::IntType>(a.element->value).width_bits) == 32,
				"element should be i32"
			);

			// The element node points straight at the cached element conversion
			// rather than at a fresh clone.
			const auto& elem_conv = queryConv(ctx, directOf(i32Of(ctx)));
			assertTrue(
				a.element.get() == &*elem_conv,
				"array element should reference the cached element conversion"
			);

			// Rejected: zero-length arrays and arrays of non-C-compatible elements.
			const StaticArrayAbstractType zero_len
				= ctx.query<QueryStaticArrayType>({ directOf(i32Of(ctx)), 0 });
			expectRejected(ctx, directOf(zero_len), "zero-length array");
			const StaticArrayAbstractType ref_array
				= ctx.query<QueryStaticArrayType>({ refOf(i32Of(ctx)), 4 });
			expectRejected(ctx, directOf(ref_array), "array of ref");
		});
	}

	void rejectedTypesTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const auto i32_type = i32Of(ctx);
			const auto i128_type
				= getIntegralType(ctx, 128, IntegralAbstractType::Signedness::Signed);
			const PointerAbstractType typed_pointer
				= ctx.query<QueryPointerType>({ directOf(i32_type) });
			const ManyPointerAbstractType many_pointer
				= ctx.query<QueryManyPointerType>({ directOf(i32_type) });
			const DynamicArrayAbstractType dynamic_array
				= ctx.query<QueryDynamicArrayType>(directOf(i32_type));
			const TupleAbstractType tuple
				= ctx.query<QueryTupleType>({ { directOf(i32_type), directOf(i32_type) } });
			const FunctionAbstractType function
				= ctx.query<QueryFunctionType>({ {}, directOf(getUnitType()) });

			const std::vector<std::pair<SymbolType<>, std::string>> rejected = {
				{ directOf(i128_type), "i128" },
				{ directOf(getRawPointerType(false)), "raw pointer" },
				{ directOf(typed_pointer), "typed pointer" },
				{ directOf(many_pointer), "many-pointer" },
				{ refOf(i32_type), "ref" },
				{ boxOf(i32_type), "box" },
				{ directOf(getUnitType()), "unit" },
				{ directOf(dynamic_array), "dynamic array" },
				{ directOf(tuple), "tuple" },
				{ directOf(function), "function" },
			};
			for (const auto& [symbol_type, what]: rejected) expectRejected(ctx, symbol_type, what);
		});
	}

public:
	~CAbiConverterTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/tsl/tests/")
