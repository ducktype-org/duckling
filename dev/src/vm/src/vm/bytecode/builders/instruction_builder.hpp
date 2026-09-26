#pragma once

#include "../instructions.hpp"

#include <base/extend_cpp/stringifyable_enum.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <cstdint>
#include <vector>


/**
 * @brief Builder-level instruction kind to set which instruction to build.
 */
// Disable liting, because of invalid naming convention.
// NOLINTBEGIN
// clang-format off
MAKE_STRINGIFYABLE_ENUM(vm::code::builders, std::uint8_t, OpKind,
	init,
	deinit,
	mov,
	cmov,
	add,
	sub,
	mul,
	div,
	mod,
	umul,
	udiv,
	umod,
	neg,
  	log_and,
	log_or,
	log_xor,
	log_not,

	bit_and,
    bit_or,
    bit_xor,
    bit_not,
    shl,
    shr,

	fadd,
	fsub,
	fmul,
	fdiv,
	fneg,

	cmpNull,
	setNull,
	cmpEq,
	cmpNeq,
	cmpGt,
	cmpGe,
	ucmpGt,
	ucmpGe,
	cmpLt,
	cmpLe,
	ucmpLt,
	ucmpLe,
	
	fcmpEq,
	fcmpNeq,
	fcmpGt,
	fcmpGe,
	fcmpLt,
	fcmpLe,

	sext,
	zext,
	trunc,
	sitofp,
	uitofp,
	fptosi,
	fptoui,
	fptrunc,
	fpext,
	
	jmp,
	jmpIf,
	jmpIfNot,
	call,
	ret,
	ret_tailcall,
	input,
	output,
	strOutput,
	alloc,
	free,
	load,
	store,
	setVTable,
	cast,
	ptrParts,
	fixedSizeTableStore,
	fixedSizeTableLoad,
	fixedSizeTableLea,
	structStore,
	structLoad,
	structLea,
	dynTableStore,
	dynTableLoad,
	dynTableLea,
	dynTableReAlloc,
	fstToDynTable,
	ref,
	upcast,
	downcast,
	virtual_call,
	variantGetInner,
	variantSetInner,

	exit
)
// clang-format on
// NOLINTEND

namespace vm::code::builders {
	/**
	 * @brief Construct fat bytecode instruction from name and arg variant vector.
	 * Expects that the instruction exists, has the correct arity, and argument types match,
	 * panics if arguments are invalid.
	 */
	vm::code::Instruction makeInstructionFromArgs(
		base::StrID name, const std::vector<opargs::OpCodeArg>& args
	);

	/**
	 * @brief Helper to compose bytecode instructions.
	 * It supports creating all available opcodes.
	 *
	 * Some operations support more arguments than their corresponding opcodes:
	 * * In case of `load` and `store`, third argument gets its own `ext` opcode.
	 */
	class InstructionBuilder final {
		std::vector<vm::opargs::OpCodeArg> args;
		OpKind                             kind{};
		bool                               kind_set = false;

	public:
		InstructionBuilder() = default;
		InstructionBuilder(OpKind kind);

		template<class... Args>
		InstructionBuilder(OpKind kind, Args&&... args): InstructionBuilder(kind) {
			pushArgs(std::forward<Args>(args)...);
		}

		void setKind(OpKind kind);

		void pushArg(const vm::opargs::OpCodeArg& arg);

		template<class... Args>
		void pushArgs(Args&&... args) {
			(pushArg(std::forward<Args>(args)), ...);
		}

		template<std::ranges::input_range R>
		void pushArgs(R&& range) {  // NOLINT
			for (auto&& arg: range) pushArg(std::forward<decltype(arg)>(arg));
		}

		[[nodiscard]] Instruction build() const;
	};
}
