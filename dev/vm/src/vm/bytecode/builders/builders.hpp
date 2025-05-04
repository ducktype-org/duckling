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
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

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
	neg,
	cmpEq,
	cmpG,
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
	class TypeContextBuilder;

	/**
	 * @brief TypeContext contains built types.
	 * It can be used built using TypesContext<TypesContextState::AddingTypes>.
	 */
	class TypeContext {
		friend TypeContextBuilder;
		TypeContext() = default;

		Box<TypeMetadata>               metadata = makeBox<TypeMetadata>();
		StableTypeIdNameMap<TypeOfData> types;

	public:
		[[nodiscard]] const StableTypeIdNameMap<TypeOfData>& getTypes() const;

		[[nodiscard]] const TypeMetadata& getMetadata() const;

		Box<TypeMetadata> moveMetadata() &&;
	};

	/**
	 * @brief TypeContextBuilder allows for adding types.
	 * It is used to build TypeContext.
	 */
	class TypeContextBuilder {
		StableTypeIdNameMap<TypeOfData> types;

		/*
		 * @brief Throws a builder error if type is invalid.
		 */
		void validateType(const TypeOfData& type) const;

		/*
		 * @brief Throws a builder error if types are invalid.
		 * Checks each type individually and inheritance
		 * hierarchy soundness.
		 */
		void validateTypes() const;

	public:
		void                                   addType(const TypeOfData& type);
		const StableTypeIdNameMap<TypeOfData>& getTypes() const;

		/**
		 * @brief Builds currently added types by building them.
		 */
		TypeContext build() const;
	};

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

	/**
	 * @brief Helper to compose bytecode functions.
	 */
	class FunctionBuilder {
		std::vector<Instruction> instructions{};
		base::StrID              name;
		const TypeContext&       type_context;
		FunctionType             type;

		/**
		 * @brief Represents a local stack variable.
		 */
		struct LocalStackEntry {
			base::StrID      local_name;
			CRef<TypeOfData> type;

			bool operator==(const LocalStackEntry& other) const {
				return local_name == other.local_name
				    && code::typeName(*type) == code::typeName(*other.type);
			}
		};

		using InstructionIter = decltype(instructions)::const_iterator;

		std::vector<LocalStackEntry>                      stack_state;
		base::HashMap<base::StrID, CRef<TypeOfData>>      local_name_to_type;
		base::HashMap<base::StrID, decltype(stack_state)> stack_at_label;
		base::HashMap<base::StrID, InstructionIter>       instruction_at_label;

		void validateInstruction(const Instruction& instruction) const;

		InstructionIter getLabelTarget(opargs::Label label) const;

		void pushStackState(opargs::StackLocalAny local, opargs::Type type);
		void popStackState();
		void popCallArgs(opargs::FunctionName function);
		void validateReturnValue() const;
		void validate();

	public:
		FunctionBuilder(base::StrID name, const TypeContext& types);

		/**
		 * @brief Adds instruction to the function.
		 * @note It can throw exceptions.
		 */
		void addInstruction(const Instruction& instruction);

		/**
		 * @brief Builds and adds instruction(s) to the function.
		 * @note It can throw exceptions.
		 */
		void addInstruction(const InstructionBuilder& instruction);

		[[nodiscard]] Function build();
	};

}
