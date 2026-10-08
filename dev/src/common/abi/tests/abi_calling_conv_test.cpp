// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
namespace at = abi::types;

namespace {
	// `i` is an unsigned integer, `si` a signed one. The sign only matters for the
	// sign/zero-extension flags: everything else classifies the two the same way.
	at::AbiType i(u64 width_bits) { return at::intType(width_bits, false); }

	at::AbiType si(u64 width_bits) { return at::intType(width_bits, true); }

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

	/**
	 * FunctionType stores AbiTypeCRef (non-owning CRef). The arena keeps the
	 * pointed-to AbiTypes alive with stable addresses (deque never relocates).
	 */
	class TypeArena {
		std::deque<at::AbiType> store;

	public:
		at::AbiTypeCRef add(at::AbiType type) {
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
		TESTER_ADD_TEST(x86Test);
		TESTER_ADD_TEST(aarch64Test);
		TESTER_ADD_TEST(extensionTest);
	}

private:
	// --- Assertion helpers ---------------------------------------------------
	static std::string dump(const at::AbiType& t) {
		return std::visit(
			[](auto&& v) -> std::string {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, at::IntType>)
					return "i" + std::to_string(v.width_bits);
				else if constexpr (std::is_same_v<T, at::FloatType>)
					return "f" + std::to_string(v.width_bits);
				else if constexpr (std::is_same_v<T, at::StructType>) {
					std::string s = "{";
					for (usize k = 0; k < v.fields.size(); k++) {
						if (k) s += ",";
						s += dump(*v.fields.at(k));
					}
					return s + "}";
				} else if constexpr (std::is_same_v<T, at::ArrayType>)
					return "[" + std::to_string(v.count) + "x" + dump(*v.element) + "]";
				else
					return "?";
			},
			t.value
		);
	}

	void expectByValue(const cc::ArgInfo& info, const at::AbiType& expected, std::string_view ctx) {
		const auto* by_value = std::get_if<cc::ArgInfo::ByValue>(&info.kind);
		assertTrue(by_value != nullptr, std::string(ctx) + ": expected ByValue");
		assertTrue(
			by_value->coerce_to_type == expected,
			std::string(ctx) + ": coerce type mismatch, got " + dump(by_value->coerce_to_type)
				+ " want " + dump(expected)
		);
	}

	void expectByPointer(const cc::ArgInfo& info, bool by_val, std::string_view ctx) {
		const auto* by_pointer = std::get_if<cc::ArgInfo::ByPointer>(&info.kind);
		assertTrue(by_pointer != nullptr, std::string(ctx) + ": expected ByPointer");
		assertTrue(
			by_pointer->by_val == by_val,
			std::string(ctx) + ": by_val mismatch, got " + std::to_string(by_pointer->by_val)
				+ " want " + std::to_string(by_val)
		);
	}

	// Computes the info for a single-argument function and returns the arg entry.
	cc::ArgInfo x86Arg(TypeArena& arena, at::AbiType arg) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType                ft{ .return_type = arena.add(at::intType(32, false)),
			                                .param_types = { arena.add(std::move(arg)) } };
		return std::move(abi->computeInfo(ft).param_info.at(0).info);
	}

	cc::ReturnEntry x86Return(TypeArena& arena, at::AbiType ret) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(std::move(ret)), .param_types = {} };
		return std::move(abi->computeInfo(ft).return_info).value();
	}

	cc::ArgInfo aarch64Arg(TypeArena& arena, at::AbiType arg) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::AArch64ABIInfo>();
		cc::FunctionType                ft{ .return_type = arena.add(at::intType(32, false)),
			                                .param_types = { arena.add(std::move(arg)) } };
		return std::move(abi->computeInfo(ft).param_info.at(0).info);
	}

	cc::ReturnEntry aarch64Return(TypeArena& arena, at::AbiType ret) {
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::AArch64ABIInfo>();
		cc::FunctionType ft{ .return_type = arena.add(std::move(ret)), .param_types = {} };
		return std::move(abi->computeInfo(ft).return_info).value();
	}

	// These two go through the public entry point rather than an ABI class directly, so
	// they also cover the `triple.os` dispatch inside `computeCallingConv`.
	cc::ArgInfo dispatchArg(TypeArena& arena, const abi::TargetABI& target, at::AbiType arg) {
		cc::FunctionType ft{ .return_type = arena.add(at::intType(32, false)),
			                 .param_types = { arena.add(std::move(arg)) } };
		return std::move(cc::computeCallingConv(target, ft).param_info.at(0).info);
	}

	cc::ReturnEntry dispatchReturn(TypeArena& arena, const abi::TargetABI& target, at::AbiType ret) {
		cc::FunctionType ft{ .return_type = arena.add(std::move(ret)), .param_types = {} };
		return std::move(cc::computeCallingConv(target, ft).return_info).value();
	}

	void expectExt(const cc::ArgInfo& info, bool sign_ext, bool zero_ext, const std::string& ctx) {
		const auto* by_value = std::get_if<cc::ArgInfo::ByValue>(&info.kind);
		assertTrue(by_value != nullptr, ctx + ": expected ByValue");
		assertTrue(
			by_value->sign_ext == sign_ext,
			ctx + ": sign_ext mismatch, got " + std::to_string(by_value->sign_ext) + " want "
				+ std::to_string(sign_ext)
		);
		assertTrue(
			by_value->zero_ext == zero_ext,
			ctx + ": zero_ext mismatch, got " + std::to_string(by_value->zero_ext) + " want "
				+ std::to_string(zero_ext)
		);
	}

	/**
	 * Runs the whole scalar table against one target, once as a parameter and once as a
	 * return value. `extends` picks the expected column: Apple arm64 and x86-64 System V
	 * make the caller extend sub-word integers to 32 bits, plain aarch64-linux does not.
	 *
	 * The expected values are what clang emits for the same C signatures - checked with
	 * `clang --target=<triple> -emit-llvm` for arm64-apple-macosx, aarch64-linux-gnu and
	 * x86_64-linux-gnu. Note `char`: signed (so sign_ext) on both Apple arm64 and x86-64.
	 */
	void checkScalarExtensions(const abi::TargetABI& target, bool extends, const std::string& name) {
		TypeArena arena;

		auto check = [&](auto make_type, bool sign_ext, bool zero_ext, const std::string& what) {
			expectExt(
				dispatchArg(arena, target, make_type()), sign_ext, zero_ext, name + " param " + what
			);
			auto ret = dispatchReturn(arena, target, make_type());
			assertFalse(ret.passed_as_param, name + " return " + what + " not sret");
			expectExt(ret.info, sign_ext, zero_ext, name + " return " + what);
		};

		check([] { return si(8); }, extends, false, "i8 signed");
		check([] { return i(8); }, false, extends, "i8 unsigned");
		check([] { return si(16); }, extends, false, "i16 signed");
		check([] { return i(16); }, false, extends, "i16 unsigned");
		// 32 bits and wider are already register-width: never extended anywhere.
		check([] { return si(32); }, false, false, "i32 signed");
		check([] { return i(32); }, false, false, "i32 unsigned");
		check([] { return si(64); }, false, false, "i64 signed");
		check([] { return i(64); }, false, false, "i64 unsigned");
		check([] { return f(32); }, false, false, "f32");
		check([] { return f(64); }, false, false, "f64");
		check([] { return p(); }, false, false, "ptr");
		check([] { return at::charType(); }, extends, false, "char");
		check([] { return at::boolType(); }, false, extends, "bool");
	}

	void x86Test() {
		TypeArena arena;

		/**
		 * @note: Sometimes clang emit the 48 bytes, but we always round up to the power of 2,
		 * so in the test we assure that we have 64 bytes in those cases.
		 */
		expectByValue(x86Arg(arena, i(8)), i(8), "i8");
		expectByValue(x86Arg(arena, i(16)), i(16), "i16");
		expectByValue(x86Arg(arena, i(32)), i(32), "i32");
		expectByValue(x86Arg(arena, i(64)), i(64), "i64");
		expectByValue(x86Arg(arena, f(32)), f(32), "f32");
		expectByValue(x86Arg(arena, f(64)), f(64), "f64");

		expectByValue(x86Arg(arena, s(i(8), i(8))), s(i(16)), "{i8,i8}");
		expectByValue(x86Arg(arena, s(i(8), i(16))), s(i(32)), "{i8,i16}");
		expectByValue(x86Arg(arena, s(i(8), i(16), i(16))), s(i(64)), "{i8,i16,i16}");
		expectByValue(x86Arg(arena, s(s(i(16), i(8)), i(8))), s(i(64)), "{{i16,i8},i8}");
		expectByValue(x86Arg(arena, s(i(16), i(16), i(32))), s(i(64)), "{i16,i16,i32}");
		expectByValue(x86Arg(arena, s(i(16), i(32), i(32))), s(i(64), i(32)), "{i16,i32,i32}");
		expectByValue(x86Arg(arena, s(i(16), i(16), i(64))), s(i(64), i(64)), "{i16,i16,i64}");
		expectByValue(
			x86Arg(arena, s(i(16), i(16), i(32), i(32))), s(i(64), i(32)), "{i16,i16,i32,i32}"
		);
		expectByValue(
			x86Arg(arena, s(i(16), i(16), i(32), i(32), i(8))),
			s(i(64), i(64)),
			"{i16,i16,i32,i32,i8}"
		);
		expectByValue(
			x86Arg(arena, s(i(16), i(16), i(16), i(16), i(16), i(8))),
			s(i(64), i(32)),
			"{i16 x5, i8}"
		);
		expectByValue(
			x86Arg(arena, s(i(16), i(16), i(16), i(16), i(16))), s(i(64), i(16)), "{i16 x5}"
		);
		expectByValue(x86Arg(arena, s(f(32))), s(f(32)), "{f32} -> SSE");
		expectByValue(x86Arg(arena, s(i(32), i(32))), s(i(64)), "{i32,i32}");
		expectByValue(x86Arg(arena, s(f(32), f(32))), s(f(64)), "{f32,f32}");
		expectByValue(x86Arg(arena, s(f(32), i(32))), s(i(64)), "{f32,i32}");
		expectByValue(x86Arg(arena, s(s(i(32), i(8)), i(8))), s(i(64), i(8)), "{{i32,i8},i8}");
		expectByValue(x86Arg(arena, s(i(32), i(16))), s(i(64)), "{i32,i16}");
		expectByValue(x86Arg(arena, s(i(64), i(8))), s(i(64), i(8)), "{i64,i8}");
		expectByValue(x86Arg(arena, s(i(64), i(16))), s(i(64), i(16)), "{i64,i16}");
		expectByValue(x86Arg(arena, s(i(64), i(32))), s(i(64), i(32)), "{i64,i32}");
		expectByValue(x86Arg(arena, s(i(64), i(64))), s(i(64), i(64)), "{i64,i64}");
		expectByValue(x86Arg(arena, s(i(64), s(i(32), i(32)))), s(i(64), i(64)), "{i64,{i32,i32}}");
		expectByPointer(x86Arg(arena, s(i(64), i(64), i(8))), /*by_val=*/true, "{i64,i64,i8}");
		expectByValue(x86Arg(arena, p()), p(), "ptr");

		// Returns: small in registers, >16 bytes via sret pointer (byval dropped).
		// A scalar return takes the simple fast path: kept as-is, never sret.
		const auto ret_scalar = x86Return(arena, i(32));
		assertFalse(ret_scalar.passed_as_param, "return i32 not sret");
		expectByValue(ret_scalar.info, i(32), "return i32");
		const auto ret_ptr = x86Return(arena, p());
		assertFalse(ret_ptr.passed_as_param, "return ptr not sret");
		expectByValue(ret_ptr.info, p(), "return ptr");
		// One-eightbyte struct return collapses to a single {i64}.
		const auto ret_small = x86Return(arena, s(i(32), i(8)));
		assertFalse(ret_small.passed_as_param, "return {i32,i8} not sret");
		expectByValue(ret_small.info, s(i(64)), "return {i32,i8}");
		// Two-eightbyte struct return (<=16B) stays in registers as {i64,i16}.
		const auto ret_two = x86Return(arena, s(i(64), i(16)));
		assertFalse(ret_two.passed_as_param, "return {i64,i16} not sret");
		expectByValue(ret_two.info, s(i(64), i(16)), "return {i64,i16}");
		// >16B struct return goes via an sret pointer (by_val dropped for returns).
		const auto ret_big = x86Return(arena, s(i(64), i(64), i(8)));
		assertTrue(ret_big.passed_as_param, "return {i64,i64,i8} is sret");
		expectByPointer(ret_big.info, /*by_val=*/false, "return {i64,i64,i8}");

		// Every param classified independently and in order.
		std::unique_ptr<cc::TargetInfo> abi = std::make_unique<cc::X86_64ABIInfo>();
		cc::FunctionType                ft{
						   .return_type = arena.add(i(32)),
						   .param_types = { arena.add(i(8)), arena.add(s(i(64), i(64), i(8))), arena.add(f(64)) }
		};
		auto info = abi->computeInfo(ft);
		assertTrue(info.param_info.size() == 3, "three params");
		expectByValue(info.param_info.at(0).info, i(8), "param0 i8");
		expectByPointer(info.param_info.at(1).info, /*by_val=*/true, "param1 {i64,i64,i8}");
		expectByValue(info.param_info.at(2).info, f(64), "param2 f64");
	}

	void aarch64Test() {
		TypeArena arena;

		// Only a homogeneous float aggregate (HFA, <=4 equal float leaves) keeps
		// its shape. Everything else is classified purely by size: <=8 one word,
		// <=16 two words, >16 indirect — no per-eightbyte narrowing.
		expectByValue(
			aarch64Arg(arena, s(f(32), f(32), f(32), f(32))),
			s(f(32), f(32), f(32), f(32)),
			"{f32, f32, f32, f32}"
		);
		expectByValue(
			aarch64Arg(arena, s(f(64), f(64), f(64), f(64))),
			s(f(64), f(64), f(64), f(64)),
			"{f64, f64, f64, f64}"
		);
		expectByValue(
			aarch64Arg(arena, s(s(f(64), f(64)), f(64), f(64))),
			s(f(64), f(64), f(64), f(64)),
			"HFA nested {{f64,f64},f64,f64}"
		);
		// Single scalars pass through unchanged: a lone float is a 1-element HFA,
		// and a lone int keeps its own width (the fast path never widens scalars).
		expectByValue(aarch64Arg(arena, i(16)), i(16), "i16");
		expectByValue(aarch64Arg(arena, i(32)), i(32), "i32");
		expectByValue(aarch64Arg(arena, i(64)), i(64), "i64");
		expectByValue(aarch64Arg(arena, f(32)), f(32), "f32");
		expectByValue(aarch64Arg(arena, f(64)), f(64), "f64");
		expectByValue(aarch64Arg(arena, s(i(32), i(32))), i(64), "{i32,i32}");
		expectByValue(aarch64Arg(arena, s(i(8), i(16))), i(64), "{i8,i16}");
		expectByValue(aarch64Arg(arena, s(s(i(32), i(8)), i(8))), s(i(64), i(64)), "{{i32,i8},i8}");
		expectByValue(aarch64Arg(arena, s(s(i(16), i(8)), i(8))), i(64), "{{i16,i8},i8}");

		// Field-layout table: one word up to 8 bytes, two words up to 16.
		expectByValue(aarch64Arg(arena, s(i(8), i(16))), i(64), "{i8,i16}");
		expectByValue(aarch64Arg(arena, s(i(8), i(16), i(16))), i(64), "{i8,i16,i16}");
		expectByValue(aarch64Arg(arena, s(i(16), i(16), i(32))), i(64), "{i16,i16,i32}");
		expectByValue(aarch64Arg(arena, s(i(32), i(16))), i(64), "{i32,i16}");
		expectByValue(aarch64Arg(arena, s(i(16), i(32), i(32))), s(i(64), i(64)), "{i16,i32,i32}");
		expectByValue(aarch64Arg(arena, s(i(16), i(16), i(64))), s(i(64), i(64)), "{i16,i16,i64}");
		expectByValue(
			aarch64Arg(arena, s(i(16), i(16), i(32), i(32))), s(i(64), i(64)), "{i16,i16,i32,i32}"
		);
		expectByValue(
			aarch64Arg(arena, s(i(16), i(16), i(32), i(32), i(8))),
			s(i(64), i(64)),
			"{i16,i16,i32,i32,i8}"
		);
		expectByValue(aarch64Arg(arena, s(i(64), i(8))), s(i(64), i(64)), "{i64,i8}");
		expectByValue(aarch64Arg(arena, s(i(64), i(16))), s(i(64), i(64)), "{i64,i16}");
		expectByValue(
			aarch64Arg(arena, s(i(16), i(16), i(16), i(16), i(16))), s(i(64), i(64)), "{i16 x5}"
		);
		expectByValue(
			aarch64Arg(arena, s(i(16), i(16), i(16), i(16), i(16), i(8))),
			s(i(64), i(64)),
			"{i16 x5, i8}"
		);

		expectByPointer(
			aarch64Arg(arena, s(i(64), arr(i(8), 8), i(8))), /*by_val=*/false, "big struct"
		);

		// Returns exercise every branch of the aarch64 return classifier.
		// Scalar: simple fast path, kept as-is, never sret.
		const auto ret_scalar = aarch64Return(arena, i(32));
		assertFalse(ret_scalar.passed_as_param, "return i32 not sret");
		expectByValue(ret_scalar.info, i(32), "return i32");
		const auto ret_float = aarch64Return(arena, f(64));
		assertFalse(ret_float.passed_as_param, "return f64 not sret");
		expectByValue(ret_float.info, f(64), "return f64");
		// HFA keeps its float shape.
		const auto ret_hfa = aarch64Return(arena, s(f(32), f(32)));
		assertFalse(ret_hfa.passed_as_param, "return {f32,f32} not sret");
		expectByValue(ret_hfa.info, s(f(32), f(32)), "return {f32,f32}");
		// <=8B struct: one integer sized to the exact byte width (8B -> i64).
		const auto ret_word = aarch64Return(arena, s(i(32), i(32)));
		assertFalse(ret_word.passed_as_param, "return {i32,i32} not sret");
		expectByValue(ret_word.info, i(64), "return {i32,i32}");
		// 5-byte struct keeps its exact bit width (i40), matching clang.
		const auto ret_odd = aarch64Return(arena, s(i(8), i(8), i(8), i(8), i(8)));
		assertFalse(ret_odd.passed_as_param, "return {i8 x5} not sret");
		expectByValue(ret_odd.info, i(40), "return {i8 x5}");
		// 9..16B struct: two words.
		const auto ret_two = aarch64Return(arena, s(i(64), i(8)));
		assertFalse(ret_two.passed_as_param, "return {i64,i8} not sret");
		expectByValue(ret_two.info, s(i(64), i(64)), "return {i64,i8}");
		// >16-byte return is sret.
		const auto ret = aarch64Return(arena, s(i(64), arr(i(8), 8), i(8)));
		assertTrue(ret.passed_as_param, "big return is sret");
		expectByPointer(ret.info, /*by_val=*/false, "big return");
	}

	void extensionTest() {
		// This is the whole reason AArch64DarwinABIInfo exists: the same machine extends
		// narrow integers under Apple's ABI and leaves them alone under AAPCS64/Linux.
		checkScalarExtensions(abi::aarch64Darwin(), /*extends=*/true, "aarch64-darwin");
		checkScalarExtensions(abi::aarch64Linux(), /*extends=*/false, "aarch64-linux");
		// x86-64 System V has always extended, but nothing asserted it before.
		checkScalarExtensions(abi::x86_64Linux(), /*extends=*/true, "x86_64-linux");
	}
};

TESTER_COMMON_MAIN("/src/common/abi/calling_conv/tests/");
