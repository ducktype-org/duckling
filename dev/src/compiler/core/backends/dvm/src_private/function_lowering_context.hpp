#pragma once

#include "dvm_value.hpp"
#include "typesystem/lower/type_layout.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <base/pointers/ref.hpp>

#include "vm/bytecode/builders/instruction_builder.hpp"
#include "vm/bytecode/instructions.hpp"
#include "vm/bytecode/type_of_data.hpp"
#include <vm/bytecode/bytecode.hpp>
#include <vm/utils/interpret.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext;

	class FunctionLoweringContext {
	public:
		FunctionLoweringContext(
			ProgramLoweringContext&                   program_context,
			base::StrID                               name,
			CRef<tsl::TypeLayout>                     return_type,
			const std::vector<CRef<tsl::TypeLayout>>& parameter_types
		);

		FunctionLoweringContext(const FunctionLoweringContext&)            = delete;
		FunctionLoweringContext(FunctionLoweringContext&&)                 = delete;
		FunctionLoweringContext& operator=(const FunctionLoweringContext&) = delete;
		FunctionLoweringContext& operator=(FunctionLoweringContext&&)      = delete;

		base::StrID getBlockLabel(lir::BlockRef block);

		const DVMLocal&               insertLirLocal(lir::LIRLocalRef local);
		[[nodiscard]] const DVMLocal& getLirLocal(lir::LIRLocalRef local) const;

		DVMValue lowerLirValue(const lir::LIRValue& lir_value);

		void beginBlock(lir::BlockRef block);

		void pushTerminator(const lir::Instruction& lir_terminator);
		void pushInstruction(const lir::Instruction& lir_instruction);

		/**
		 * @brief Registers LIR function parameter as a DVM function parameter.
		 * @param lir_func_param LIR local representing a function parameter.
		 * In reality this just means we can use this "already present" local.
		 */
		void registerFunctionParameter(lir::LIRLocalRef lir_func_param);

		DVMLocal getFunctionReturnValueLocal();

		vm::code::Function finish() &&;

	private:
		// Creates a mapping between a LIR local and DVM local.
		const DVMLocal& createLirLocalToDVMMapping(lir::LIRLocalRef local);

		void pushInstruction(const vm::code::Instruction& instruction);

		void pushInstruction(const vm::code::builders::InstructionBuilder& instruction);

		void  handleFunctionCall(
			 const lir::FunctionLiteral&  called_function,
			 const DVMValue&              called_func_name,
			 const std::vector<DVMValue>& func_args,
			 base::Optional<DVMValue>     output
		 );

		usize next_temp_id = 0;
		DVMLocal pushTempLocal(
			const vm::code::TypeOfData& type, base::Optional<const char*> name_hint = {}
		);

		ProgramLoweringContext& program_context;

		base::Map<lir::LIRLocalRef, DVMLocal> lir_local_to_dvm;
		base::Map<lir::BlockRef, base::StrID> block_to_label;

		vm::code::TypeOfData               function_return_type;
		std::vector<vm::code::TypeOfData>  function_parameter_types;
		base::StrID                        function_name;
		std::vector<vm::code::Instruction> function_body;
	};
}
