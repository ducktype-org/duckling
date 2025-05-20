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

		void validateArgTypes(const Instruction& instruction, const LocalStack& current_stack) const;
		void validateSpecificInstruction(const Instruction& instruction) const;
		void validateExtension(usize instruction_index) const;
		void validateInstruction(const Instruction& instruction, const LocalStack& current_stack)
			const;
		void validateArgInstantiable(const opargs::Type& arg) const;
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
