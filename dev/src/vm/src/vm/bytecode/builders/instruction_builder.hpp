#pragma once

#include "../instructions.hpp"

#include <base/maps.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <base/stringifyable_enum.hpp>
#include <base/strongly_typed_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <cstdint>
#include <utility>
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
	cmpEq,
	cmpG,
	ucmpG,
	jmp,
	jmpIf,
	jmpIfNot,
	call,
	ret,
	ret_tailcall,
	input,
	output,
	alloc,
	free,
	load,
	store,
	realloc,
	setVTable,

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
	 * * In case of `load` and `store`, third argument gets its own `ext` opcode.
	 */
	class InstructionBuilder {
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

		[[nodiscard]] std::vector<Instruction> build() const;
	};
}
