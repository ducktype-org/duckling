#include <abi/calling_conv/calling_conv.hpp>
#include <abi/target.hpp>
#include <abi/type_system/type.hpp>

#include <tester/tester.hpp>

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cc = abi::calling_conv;
namespace at = abi::type_system;

namespace {

	// --- Type builders -------------------------------------------------------
	// Short helpers so a case reads close to its C spelling. Ints default to
	// unsigned, matching what the calling-conv code emits for coerced values.
	at::AbiType i(u64 width_bits) { return at::intType(width_bits, false); }

	at::AbiType f(u64 width_bits) { return at::floatType(width_bits); }

	at::AbiType p() { return at::pointerType(); }

	template<class... Ts>
	std::vector<at::AbiTypePtr> fieldsOf(Ts&&... types) {
		std::vector<at::AbiTypePtr> out;
		out.reserve(sizeof...(Ts));
		(out.push_back(at::makeBoxAbiType(std::forward<Ts>(types))), ...);
		return out;
	}

	// Struct-of: `s(i(32), i(8))` == C `struct { i32; i8; }`.
	template<class... Ts>
	at::AbiType s(Ts&&... types) {
		return at::structType(fieldsOf(std::forward<Ts>(types)...));
	}

	at::AbiType arr(at::AbiType element, usize count) {
		return at::arrayType(at::makeBoxAbiType(std::move(element)), count);
	}

	// --- Arena ---------------------------------------------------------------
	// FunctionType stores AbiTypeRef (non-owning CRef). The arena keeps the
	// pointed-to AbiTypes alive with stable addresses (deque never relocates).
	class TypeArena {
		std::deque<at::AbiType> store;

	public:
		at::AbiTypeRef add(at::AbiType type) {
			store.push_back(std::move(type));
			return { &store.back() };
		}
	};

}

class AbiCallingConvTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS AbiCallingConvTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// x86_64 arguments.
		TESTER_ADD_TEST(x86ScalarSmallIntTest);
		TESTER_ADD_TEST(x86SmallStructTest);
		TESTER_ADD_TEST(x86ScalarPointerTest);
		TESTER_ADD_TEST(x86ScalarDoubleTest);
		TESTER_ADD_TEST(x86TwoIntsOneWordTest);
		TESTER_ADD_TEST(x86TwoFloatsPackTest);
		TESTER_ADD_TEST(x86FloatAndIntEightbyteTest);
		TESTER_ADD_TEST(x86NestedStructIntIntTest);
		TESTER_ADD_TEST(x86HighEightbyteWidthTest);
		TESTER_ADD_TEST(x86TwoRegsFullTest);
		TESTER_ADD_TEST(x86LargeStructByPointerTest);
		// x86_64 returns.
		TESTER_ADD_TEST(x86ReturnSmallStructTest);
		TESTER_ADD_TEST(x86ReturnLargeStructSretTest);
		// aarch64 arguments.
		TESTER_ADD_TEST(aarch64ScalarIntTest);
		TESTER_ADD_TEST(aarch64HfaFourFloatsTest);
		TESTER_ADD_TEST(aarch64HomogeneousTwoIntsTest);
		TESTER_ADD_TEST(aarch64NestedStructTwoWordsTest);
		TESTER_ADD_TEST(aarch64NonHomogeneousSmallTest);
		TESTER_ADD_TEST(aarch64LargeStructByPointerTest);
		// aarch64 returns.
		TESTER_ADD_TEST(aarch64ReturnLargeStructSretTest);
		// param-list wiring.
		TESTER_ADD_TEST(x86MultipleParamsTest);
	}

private:
	// --- Assertion helpers ---------------------------------------------------
	void expectByValue(
		const cc::ArgInfo& info, const at::AbiType& expected, std::string_view ctx
	) {
		const auto* by_value = std::get_if<cc::ArgInfo::ByValue>(&info.kind);
		assertTrue(by_value != nullptr, std::string(ctx) + ": expected ByValue");
		assertTrue(by_value->coerce_to_type == expected, std::string(ctx) + ": coerce type mismatch");
	}

	void expectByPointer(const cc::ArgInfo& info, bool by_val, std::string_view ctx) {
		const auto* by_pointer = std::get_if<cc::ArgInfo::ByPointer>(&info.kind);
		assertTrue(by_pointer != nullptr, std::string(ctx) + ": expected ByPointer");
		assertTrue(by_pointer->by_val == by_val, std::string(ctx) + ": by_val mismatch");
	}

	// Computes the info for a single-argument function and returns the arg entry.
	cc::ArgInfo x86Arg(TypeArena& arena, at::AbiType arg) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(at::intType(32, false)),
			                 .param_types = { arena.add(std::move(arg)) } };
		return std::move(abi->computeInfo(ft).param_info.at(0).info);
	}

	cc::ReturnEntry x86Return(TypeArena& arena, at::AbiType ret) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(std::move(ret)), .param_types = {} };
		return std::move(abi->computeInfo(ft).return_info);
	}

	cc::ArgInfo aarch64Arg(TypeArena& arena, at::AbiType arg) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::AArch64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(at::intType(32, false)),
			                 .param_types = { arena.add(std::move(arg)) } };
		return std::move(abi->computeInfo(ft).param_info.at(0).info);
	}

	cc::ReturnEntry aarch64Return(TypeArena& arena, at::AbiType ret) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::AArch64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(std::move(ret)), .param_types = {} };
		return std::move(abi->computeInfo(ft).return_info);
	}

	// --- x86_64 arguments ----------------------------------------------------

	void x86ScalarSmallIntTest() {
		// i8 fits in <8 bytes → coerced to a raw integer of its bit width.
		TypeArena arena;
		expectByValue(x86Arg(arena, i(8)), i(8), "x86 i8");
	}

	void x86SmallStructTest() {
		// struct{i8,i8} is 2 bytes → one integer of 16 bits.
		TypeArena arena;
		expectByValue(x86Arg(arena, s(i(8), i(8))), i(16), "x86 {i8,i8}");
	}

	void x86ScalarPointerTest() {
		// Pointer is 8 bytes → single-eightbyte integer struct.
		TypeArena arena;
		expectByValue(x86Arg(arena, p()), s(i(64)), "x86 ptr");
	}

	void x86ScalarDoubleTest() {
		// f64 fills one SSE eightbyte → struct{double}.
		TypeArena arena;
		expectByValue(x86Arg(arena, f(64)), s(f(64)), "x86 f64");
	}

	void x86TwoIntsOneWordTest() {
		// struct{i32,i32} is 8 bytes → single INTEGER eightbyte → struct{i64}.
		TypeArena arena;
		expectByValue(x86Arg(arena, s(i(32), i(32))), s(i(64)), "x86 {i32,i32}");
	}

	void x86TwoFloatsPackTest() {
		// struct{f32,f32} packs into one SSE eightbyte spanning 8 bytes → double.
		TypeArena arena;
		expectByValue(x86Arg(arena, s(f(32), f(32))), s(f(64)), "x86 {f32,f32}");
	}

	void x86FloatAndIntEightbyteTest() {
		// A float and an int sharing an eightbyte classify as INTEGER → struct{i64}.
		TypeArena arena;
		expectByValue(x86Arg(arena, s(f(32), i(32))), s(i(64)), "x86 {f32,i32}");
	}

	void x86NestedStructIntIntTest() {
		// The requested case: struct{ struct{i32,i8}, i8 }.
		// Layout: i32@0, i8@4, i8@8 → data extent 9. Eightbyte 0 (bytes 0..8) is
		// packed INTEGER → i64; eightbyte 1 holds only c@8 (1 byte) → i8.
		// Matches clang `take_nested(i64, i8)` (see scripts/abi_clang_check.sh).
		TypeArena arena;
		expectByValue(x86Arg(arena, s(s(i(32), i(8)), i(8))), s(i(64), i(8)), "x86 {{i32,i8},i8}");
	}

	void x86HighEightbyteWidthTest() {
		// A single trailing field in the high eightbyte sizes that register to the
		// field's own (rounded-up) width, not to the padded struct size.
		// clang: t9(i64,i8), t10(i64,i16), t12(i64,i32). See abi_clang_check.sh.
		TypeArena arena;
		expectByValue(x86Arg(arena, s(i(64), i(8))), s(i(64), i(8)), "x86 {i64,i8}");
		expectByValue(x86Arg(arena, s(i(64), i(16))), s(i(64), i(16)), "x86 {i64,i16}");
		expectByValue(x86Arg(arena, s(i(64), i(32))), s(i(64), i(32)), "x86 {i64,i32}");
	}

	void x86TwoRegsFullTest() {
		// struct{i64,i64} is exactly 16 bytes → two INTEGER regs, NOT indirect.
		// clang: `a(i64, i64)` (see abi_clang_check.sh).
		TypeArena arena;
		expectByValue(x86Arg(arena, s(i(64), i(64))), s(i(64), i(64)), "x86 {i64,i64}");
	}

	void x86LargeStructByPointerTest() {
		// struct{i64,i64,i8} is 24 bytes (>16) → passed indirectly with byval.
		TypeArena arena;
		expectByPointer(x86Arg(arena, s(i(64), i(64), i(8))), /*by_val=*/true, "x86 {i64,i64,i8}");
	}

	// --- x86_64 returns ------------------------------------------------------

	void x86ReturnSmallStructTest() {
		TypeArena  arena;
		const auto ret = x86Return(arena, s(i(32), i(8)));
		assertFalse(ret.passed_as_param, "x86 return {i32,i8} not sret");
		expectByValue(ret.info, s(i(64)), "x86 return {i32,i8}");
	}

	void x86ReturnLargeStructSretTest() {
		// >16-byte return goes through a hidden pointer param; the byval attribute
		// is dropped for the sret pointer.
		TypeArena  arena;
		const auto ret = x86Return(arena, s(i(64), i(64), i(8)));
		assertTrue(ret.passed_as_param, "x86 return {i64,i64,i8} is sret");
		expectByPointer(ret.info, /*by_val=*/false, "x86 return {i64,i64,i8}");
	}

	// --- aarch64 arguments ---------------------------------------------------

	void aarch64ScalarIntTest() {
		// A single int flattens to one leaf → homogeneous → struct{that int}.
		TypeArena arena;
		expectByValue(aarch64Arg(arena, i(32)), s(i(32)), "aarch64 i32");
	}

	void aarch64HfaFourFloatsTest() {
		// Four equal floats form an HFA → preserved as struct{f32 x4}.
		TypeArena arena;
		expectByValue(
			aarch64Arg(arena, s(f(32), f(32), f(32), f(32))),
			s(f(32), f(32), f(32), f(32)),
			"aarch64 HFA<f32,4>"
		);
	}

	void aarch64HomogeneousTwoIntsTest() {
		// struct{i32,i32} → homogeneous → struct{i32,i32}.
		TypeArena arena;
		expectByValue(aarch64Arg(arena, s(i(32), i(32))), s(i(32), i(32)), "aarch64 {i32,i32}");
	}

	void aarch64NestedStructTwoWordsTest() {
		// struct{ struct{i32,i8}, i8 }: non-homogeneous, 12 bytes → 8..16 range →
		// two 64-bit words.
		TypeArena arena;
		expectByValue(
			aarch64Arg(arena, s(s(i(32), i(8)), i(8))), s(i(64), i(64)), "aarch64 {{i32,i8},i8}"
		);
	}

	void aarch64NonHomogeneousSmallTest() {
		// struct{i8,i16}: non-homogeneous, 4 bytes (<8) → single 64-bit word.
		TypeArena arena;
		expectByValue(aarch64Arg(arena, s(i(8), i(16))), i(64), "aarch64 {i8,i16}");
	}

	void aarch64LargeStructByPointerTest() {
		// Non-homogeneous 24-byte struct (>16) → indirect (no byval on aarch64).
		TypeArena arena;
		expectByPointer(
			aarch64Arg(arena, s(i(64), arr(i(8), 8), i(8))), /*by_val=*/false, "aarch64 big struct"
		);
	}

	// --- aarch64 returns -----------------------------------------------------

	void aarch64ReturnLargeStructSretTest() {
		TypeArena  arena;
		const auto ret = aarch64Return(arena, s(i(64), arr(i(8), 8), i(8)));
		assertTrue(ret.passed_as_param, "aarch64 big return is sret");
		expectByPointer(ret.info, /*by_val=*/false, "aarch64 big return");
	}

	// --- Param-list wiring ---------------------------------------------------

	void x86MultipleParamsTest() {
		// Verify every param is classified independently and in order.
		TypeArena                       arena;
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType                ft{ .return_type = arena.add(i(32)),
			                                .param_types = { arena.add(i(8)),
			                                                 arena.add(s(i(64), i(64), i(8))),
			                                                 arena.add(f(64)) } };
		auto                            info = abi->computeInfo(ft);
		assertTrue(info.param_info.size() == 3, "three params");
		expectByValue(info.param_info.at(0).info, i(8), "param0 i8");
		expectByPointer(info.param_info.at(1).info, /*by_val=*/true, "param1 {i64,i64,i8}");
		expectByValue(info.param_info.at(2).info, s(f(64)), "param2 f64");
	}
};

TESTER_COMMON_MAIN("/src/common/abi/calling_conv/tests/");
