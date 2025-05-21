#pragma once


#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <vector>

namespace vm::code::builders {

	/*
	 * Class responsible for function validation.
	 * Processes the control-flow graph and simulates
	 * stack operations. Throws subclasses of BuilderError.
	 */
	class FunctionValidator {
		/**
		 * @brief Represents a local stack variable.
		 */
		struct LocalStackEntry {
			base::StrID      local_name;
			CRef<TypeOfData> type;

			constexpr bool operator==(const LocalStackEntry& other) const;
		};

		Identifier                      name;
		const TypeContext&              type_context;
		const GlobalDataMap&            globals;
		const std::vector<Instruction>& instructions;
		FunctionType                    type;

		class LocalStack {
		public:
			std::vector<LocalStackEntry>                 stack_state;
			base::HashMap<base::StrID, CRef<TypeOfData>> local_name_to_type;

			LocalStack(const FunctionType& type, const TypeContext& type_context);
			void push(
				const opargs::StackLocalAny& local,
				const opargs::Type&          type,
				const TypeContext&           type_context
			);
			void             pop(const instructions::Op_deinit& cause);
			bool             contains(base::StrID local_name) const;
			CRef<TypeOfData> at(base::StrID local_name) const;
			void             popCallArgs(
							const opargs::OpCodeFunctionArg& function, const TypeContext& type_context
						);
			void validateTailcall(
				const opargs::OpCodeFunctionArg& function,
				const TypeContext&               type_context,
				const FunctionType&              type
			) const;
			void castPrimitive(
				const opargs::OpCodePrimitiveArg& local,
				const opargs::Type&               type,
				const Instruction&                instruction,
				const TypeContext&                type_context
			);
		};

		std::vector<bool>                                        visited_instructions;
		base::HashMap<base::StrID, std::vector<LocalStackEntry>> stack_at_label;
		base::HashMap<base::StrID, usize>                        index_of_label;
		base::HashMap<base::StrID, std::vector<Instruction>>     jumps_to_label;
		bool                                                     validated = false;

		/**
		 * @brief Validates instruction's arguments in a trivial, generic way, i.e. if an
		 * instruction expects a pointer argument then this function validates this argument really
		 * is a pointer, not a label or a primitive. In case of this function, an instruction can be
		 * thought of as an argument collection.
		 * @param instruction Instruction that is validated.
		 */
		void validateArgTypes(const Instruction& instruction, const LocalStack& current_stack) const;

		/**
		 * @brief Validates instruction's arguments non-trivially - using specific logic for each
		 * instruction. For instance, an instruction may expect type `T` as arg0, a `Pointer<T>` as
		 * arg1 and another `Pointer<T>` as an extension. This is the place to express such logic.
		 * @param instruction Instruction that is validated.
		 * @param next_instruction Optional next instruction. Used when expecting e.g. `ext_*`.
		 * @note Presence of extensions is checked by different function: `validateExtension`.
		 */
		void validateArgTypesNonTrivially(
			const Instruction&                 instruction,
			base::Optional<const Instruction&> next_instruction,
			const LocalStack&                  current_stack
		) const;

		/**
		 * @brief Meant to be called for every instruction in a function, not just extension.
		 * In case it's an extension, it validates whether `predecessor` really expected this
		 * extension.
		 */
		void validateExtension(
			base::Optional<const Instruction&> predecessor, const Instruction& instruction
		) const;

		/**
		 * @brief Validates, whether given type is really instantiable, e.g. it's a primitive, or a
		 * real data, not an abstract class or an interface.
		 */
		void validateArgInstantiable(const opargs::Type& arg) const;

		void validateUpcast(
			const instructions::Op_upcast_lptr_lptr& instruction, const LocalStack& current_stack
		) const;

		void validateFunctionEnd() const;

		usize getLabelTarget(const opargs::Label& label) const;
		void  preprocessLabels();
		void  traverseControlFlowGraph();

	public:
		FunctionValidator(
			Identifier                      name,
			const TypeContext&              type_context,
			const GlobalDataMap&            globals,
			const std::vector<Instruction>& instructions
		);
		void                     validate();
		std::vector<Instruction> extractReachableCode();
	};
}
