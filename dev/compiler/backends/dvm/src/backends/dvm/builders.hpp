#pragma once

#include <base/ref.hpp>
#include <vm/preprocessor/parser/type_of_data.hpp>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <vm/code_data/opcode_args.hpp>
#include <base/stringifyable_enum.hpp>
#include "instructions.hpp"
#include <cstdint>
#include <deque>
#include "elements.hpp"
#include <lir/lir_structure/lir_structure.hpp>


// Disable liting, because of invalid naming convention.
// NOLINTBEGIN
// clang-format off
MAKE_STRINGIFYABLE_ENUM(compiler::backend_vm, std::uint8_t, OpKind,
	init,
	deinit,
	mov,
	cmov,
	add,
	sub,
	mul,
	div,
	mod,
	neg,
	cmpEq,
	cmpG,
	jmp,
	jmpIf,
	jmpNotIf,
	call,
	ret,
	ret_tailcall,
	input,
	output,
	alloc,
	free,
	load,
	store,

	/**
	 *  Do not use directly. If an instruction supports `ext` opcodes,
	 *  just push another argument to the instruction builder.
	 */
	ext,
	exit
)
// clang-format on
// NOLINTEND

namespace compiler::backend_vm {

	/**
	 * @brief Helper to compose bytecode instructions.
	 * It supports creating all available opcodes.
	 *
	 * Some operations support more arguments than their corresponding opcodes:
	 * * In case of arithmetic operations, 3 arguments mean first argument
	 *  	should store the result of the operation on the succeeding arguments.
	 * * In case of `neg`, 2 arguments mean the first stores the result of neg on the successor.
	 * * In case of `load` and `store`, third argument gets its own `ext` opcode.
	 */
	class InstructionBuilder {
		std::deque<vm::opargs::OpCodeArg> args;
		OpKind                            kind{};
		bool                              kind_set = false;

	public:
		InstructionBuilder() = default;
		InstructionBuilder(OpKind kind);

		void setKind(OpKind kind);

		void pushArg(const vm::opargs::OpCodeArg& arg);

		template<class... Args>
		void pushArgs(Args&&... args) {
			(pushArg(std::forward<Args>(args)), ...);
		}

		[[nodiscard]] std::deque<VmInstruction> build() const;
	};

	/**
	 * @brief Helper to compose bytecode functions.
	 */
	class FunctionBuilder {
		std::deque<VmInstruction> instructions{};
		base::StrID               name;

		/**
		 * @brief Represents a local stack variable.
		 */
		struct LocalStackEntry {
			base::StrID tp;
			usize       local_stack_position;
			usize       type_size;
		};

		std::deque<LocalStackEntry> local_stack;

		usize max_stack_size = 0;
		usize ret_size       = 0;

		const base::HashMap<base::StrID, vm::parser::TypeOfData>& available_types;

	public:
		FunctionBuilder(
			base::StrID                                               name,
			const base::HashMap<base::StrID, vm::parser::TypeOfData>& available_types
		);

		usize               initType(base::StrID tp);
		void                deinitType();
		[[nodiscard]] usize getLocalSize() const;

		void addInstruction(const VmInstruction& instruction);
		void addInstruction(const InstructionBuilder& instruction);

		[[nodiscard]] Function build() const;
	};

	/**
	 * @brief Helper to compose bytecode files.
	 */
	class CodeFileBuilder {
		std::deque<FunctionBuilder> functions{};

		std::deque<CRef<vm::parser::TypeOfData>>           types{};
		base::HashMap<base::StrID, vm::parser::TypeOfData> type_map{};

	public:
		CodeFileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::parser::TypeOfData& type);

		const base::HashMap<base::StrID, vm::parser::TypeOfData>& getAvailableTypes() const;

		[[nodiscard]] CodeFile build() const;
	};


}
