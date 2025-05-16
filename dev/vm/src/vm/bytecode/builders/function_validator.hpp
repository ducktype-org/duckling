#pragma once


#include <base/string_id.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/bytecode.hpp>

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

		std::vector<bool>                                    visited_instructions;
		std::vector<LocalStackEntry>                         stack_state;
		base::HashMap<base::StrID, CRef<TypeOfData>>         local_name_to_type;
		base::HashMap<base::StrID, decltype(stack_state)>    stack_at_label;
		base::HashMap<base::StrID, usize>                    index_of_label;
		base::HashMap<base::StrID, std::vector<Instruction>> jumps_to_label;
		bool                                                 validated = false;

		void validateExtension(usize instruction_index) const;
		void validateInstruction(const Instruction& instruction) const;
		void validateArgInstantiable(const opargs::Type& arg) const;
		void validateFunctionEnd() const;
		void validateTailcall(const opargs::OpCodeFunctionArg& function) const;

		usize getLabelTarget(const opargs::Label& label) const;
		void  initStackState();
		void  pushStackState(const opargs::StackLocalAny& local, const opargs::Type& type);
		void  popStackState(const instructions::Op_deinit& cause);
		void  popCallArgs(const opargs::OpCodeFunctionArg& function);
		void  castPrimitive(
			 const opargs::OpCodePrimitiveArg& local,
			 const opargs::Type&               type,
			 const Instruction&                instruction
		 );
		void preprocessLabels();
		void traverseControlFlowGraph();

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
