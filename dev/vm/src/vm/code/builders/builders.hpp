#pragma once

#include <base/ref.hpp>
#include <deque>
#include <vm/code/type_of_data.hpp>
#include <base/maps.hpp>
#include <base/string_id.hpp>
#include <vm/code/opcode_args.hpp>
#include <base/stringifyable_enum.hpp>
#include "../instructions.hpp"
#include <cstdint>
#include <vm/code/code.hpp>


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

namespace vm::code::builders {

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

		[[nodiscard]] std::vector<Instruction> build() const;
	};

	/**
	 * @brief Helper to compose bytecode functions.
	 */
	class FunctionBuilder {
		std::vector<Instruction> instructions{};
		base::StrID              name;

		/**
		 * @brief Represents a local stack variable.
		 */
		struct LocalStackEntry {
			base::StrID tp;
			usize       local_stack_position;
			usize       type_size;
		};

		std::vector<LocalStackEntry> local_stack;

		usize max_stack_size = 0;
		usize ret_size       = 0;

		const base::HashMap<base::StrID, vm::code::TypeOfData>& available_types;

		void handleJump(base::StrID label_name); // save stack size
		void handleLabel(base::StrID label_name) // retrieve stack from jump

	public:
		FunctionBuilder(
			base::StrID                                             name,
			const base::HashMap<base::StrID, vm::code::TypeOfData>& available_types
		);

		usize               initType(base::StrID tp);
		void                deinitType();
		[[nodiscard]] usize getLocalSize() const;

		void addInstruction(const Instruction& instruction);
		void addInstruction(const InstructionBuilder& instruction);

		[[nodiscard]] Function build() const;
	};

	/**
	 * @brief Helper to compose bytecode files.
	 */
	class CodeFileBuilder {
		std::vector<FunctionBuilder> functions{};

		std::vector<CRef<vm::code::TypeOfData>>          types{};
		base::HashMap<base::StrID, vm::code::TypeOfData> type_map{};

	public:
		CodeFileBuilder() = default;

		void addFunction(const FunctionBuilder& function);

		void addType(const vm::code::TypeOfData& type);

		const base::HashMap<base::StrID, vm::code::TypeOfData>& getAvailableTypes() const;

		[[nodiscard]] CodeFile build() const;
	};


}
