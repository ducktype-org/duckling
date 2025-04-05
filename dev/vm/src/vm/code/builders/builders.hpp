#pragma once

#include "../instructions.hpp"

#include <base/maps.hpp>
#include <base/ref.hpp>
#include <base/stable_type_id_name_map.hpp>
#include <base/string_id.hpp>
#include <base/stringifyable_enum.hpp>
#include <base/strongly_typed_id.hpp>

#include <vm/code/code.hpp>
#include <vm/code/opcode_args.hpp>
#include <vm/code/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <cstdint>
#include <deque>
#include <utility>


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

	enum class TypesContextState : bool {
		AddingTypes,
		Finalized,
	};


	template<TypesContextState state = TypesContextState::AddingTypes>
	class TypesContext;

	/**
	 * @brief TypesContext specialization that allows for adding types.
	 * It can be used to build TypesContext<TypesContextState::Finalized>.
	 */
	template<>
	class TypesContext<TypesContextState::AddingTypes> {
		base::StableTypeIdNameMap<TypeOfData> types;

	public:
		void                                         addType(const TypeOfData& type);
		const base::StableTypeIdNameMap<TypeOfData>& getTypes() const;

		/**
		 * @brief Finalizes currently added types by building them.
		 */
		TypesContext<TypesContextState::Finalized> finalized() const;
	};

	/**
	 * @brief TypesContext specialization that contains built types.
	 * It can be used built using TypesContext<TypesContextState::AddingTypes>.
	 */
	template<>
	class TypesContext<TypesContextState::Finalized> {
		friend TypesContext<TypesContextState::AddingTypes>;
		TypesContext() = default;

		Box<TypeMetadata>       metadata = makeBox<TypeMetadata>();
		std::vector<TypeOfData> types;

	public:
		[[nodiscard]] const std::vector<TypeOfData>& getTypes() const;

		[[nodiscard]] const TypeMetadata& getMetadata() const;

		Box<TypeMetadata> moveMetadata() &&;
	};

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

		STRONG_TYPEDEF_ID(LocalStackEntryID)

		/**
		 * @brief Represents a local stack variable.
		 */
		struct LocalStackEntry {
			// This is does not equal to variable index.
			// It is used to check stack state between jumps.
			LocalStackEntryID unique_id;
			base::StrID       tp;
			usize             local_stack_position;
			usize             type_size;

			bool operator==(const LocalStackEntry& other) const = default;
		};

		std::vector<LocalStackEntry> local_stack;

		usize max_stack_size = 0;
		usize ret_size       = 0;

		const TypesContext<TypesContextState::Finalized>& types_context;

		base::HashMap<base::StrID, std::vector<LocalStackEntry>> stack_state_at_label;
		base::HashMap<base::StrID, std::vector<Instruction>>     label_users;

		void saveStackState(base::StrID at_label_name);

		void handleLabel(instructions::Op_label label);
		void handleCallFunc(instructions::Op_call_func call);
		void handleDeinit();

	public:
		FunctionBuilder(base::StrID name, const TypesContext<TypesContextState::Finalized>& types);

		/**
		 * @note This is temporary, look at impl of build().
		 */
		void setRetSize(usize ret_size);

		/**
		 * @brief Return variable's stack offset. Also pushes `init_type` instruction.
		 */
		usize initType(instructions::Op_init_type init);

		[[nodiscard]] usize getLocalSize() const;

		void addInstruction(const Instruction& instruction);
		void addInstruction(const InstructionBuilder& instruction);

		[[nodiscard]] Function build() const;
	};

}
