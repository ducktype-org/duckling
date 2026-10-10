// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/str/str_utils.hpp>

#include <tester/tester.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/core/safe/low_program/micro_instruction_args.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

/**
 * Checks that every high and micro instruction is named
 * `baseName[_operand1Suffix[_operand2Suffix[_operand3Suffix]]]`: a camelCase base name followed by
 * the short name of each of its operands, in order.
 */
class InstructionNamingTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS InstructionNamingTester

	/**
	 * Pseudo-instruction marking a jump target. It is never executed, has no micro counterpart, and
	 * its operand is the label it defines, so it keeps the bare name.
	 */
	static constexpr std::array HIGH_EXCEPTIONS = { std::string_view("label") };

	/**
	 * The micro instruction takes its third operand (`pptr`) from the `ext_pptr` that follows it,
	 * as micro instructions have at most two operands.
	 */
	static constexpr std::array MICRO_EXCEPTIONS = { std::string_view("ptrParts_p64_p64_pptr") };

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testHighInstructionNames);
		TESTER_ADD_TEST(testMicroInstructionNames);
		TESTER_ADD_TEST(testNamingErrorDetectsViolations);
	}

	/**
	 * @brief Returns a description of how `name` breaks the convention, or an empty string if it
	 * follows it.
	 */
	static std::string namingError(
		const std::string_view name, const std::vector<std::string_view>& suffixes
	) {
		const auto base = name.substr(0, name.find('_'));

		const bool camel_case
			= !base.empty() && base.front() >= 'a' && base.front() <= 'z'
		   && std::ranges::all_of(base, [](const char c) {
				  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			  });
		if (!camel_case)
			return base::strConcat("`", name, "`: base name `", base, "` is not camelCase");

		std::string expected(base);
		for (const auto suffix: suffixes) expected += base::strConcat("_", suffix);
		if (name != expected)
			return base::strConcat(
				"`", name, "`: suffixes do not match the operands, expected `", expected, "`"
			);
		return {};
	}

	template<typename TInstr>
	static std::vector<std::string_view> highSuffixes() {
		return []<typename... TArgs>(std::type_identity<std::tuple<TArgs...>>) {
			return std::vector<std::string_view>{ TArgs::OP_SHORT... };
		}(std::type_identity<typename TInstr::ArgTypes>{});
	}

	void checkName(
		const std::string_view                  name,
		const std::vector<std::string_view>&    suffixes,
		const std::span<const std::string_view> exceptions
	) {
		if (std::ranges::contains(exceptions, name)) return;
		const auto error = namingError(name, suffixes);
		assertTrue(error.empty(), error, false);
	}

	void testHighInstructionNames() {
#define HANDLE_INSTR(name)                                 \
	checkName(                                             \
		vm::code::instructions::Op_##name::NAME,           \
		highSuffixes<vm::code::instructions::Op_##name>(), \
		HIGH_EXCEPTIONS                                    \
	);
#include <vm/bytecode/instruction_definitions.def.hpp>
#undef HANDLE_INSTR
	}

	void testMicroInstructionNames() {
#define HANDLE_MICRO_INSTR_0ARGS(name) checkName(#name, {}, MICRO_EXCEPTIONS);
#define HANDLE_MICRO_INSTR_1ARGS(name, arg0) \
	checkName(#name, { arg0::ARG_SHORT }, MICRO_EXCEPTIONS);
#define HANDLE_MICRO_INSTR_2ARGS(name, arg0, arg1) \
	checkName(#name, { arg0::ARG_SHORT, arg1::ARG_SHORT }, MICRO_EXCEPTIONS);
#include <vm/core/safe/low_program/micro_instruction_definitions.def.hpp>
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS
	}

	void testNamingErrorDetectsViolations() {
		ASSERT_TRUE(namingError("bitAnd_p8_imm", { "p8", "imm" }).empty());
		ASSERT_TRUE(namingError("ret", {}).empty());
		ASSERT_TRUE(namingError("init64_off_type", { "off", "type" }).empty());

		ASSERT_TRUE(!namingError("bit_and_p8_imm", { "p8", "imm" }).empty());
		ASSERT_TRUE(!namingError("check_strategy", {}).empty());
		ASSERT_TRUE(!namingError("SetNull_pptr", { "pptr" }).empty());
		ASSERT_TRUE(!namingError("call_func", { "func", "off" }).empty());
		ASSERT_TRUE(!namingError("mov_bfst_bfst", { "barr", "barr" }).empty());
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/");
