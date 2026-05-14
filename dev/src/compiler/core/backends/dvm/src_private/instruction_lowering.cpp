#include "debug_info_utils.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "operations/dvm_operation.hpp"
#include "operations/instruction_lowerer.hpp"

#include <lir/lir_structure/lir_structure.hpp>
#include <program_lowering_context.hpp>

#include <logger/logger.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

using namespace compiler::backend_vm::internal;
using namespace vm::code;
using namespace compiler;
using namespace vm::code::builders;

void FunctionLoweringContext::pushInstruction(const lir::Instruction& lir_instruction) {
	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_instruction.metadata.position, pos) {
			builder.addInstruction(instructionsCount(), mapDIPosition(pos));
		}
	}

	DVMOperation dvm_operation = lirInstrToDVMOperation(*this, lir_instruction);
	VISIT(dvm_operation, op, InstructionLowerer(this).lower(op));
	// Clean all of the temporaries created by `pushTempLocal` while lowering this instruction.
	cleanUpRegisteredTemps();
}

void FunctionLoweringContext::pushTerminator(const lir::Instruction& lir_terminator) {
	pushInstruction(instructions::Comment(base::StrID(
		base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
	)));

	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_terminator.metadata.position, pos) {
			builder.addInstruction(instructionsCount(), mapDIPosition(pos));
		}
	}

	DVMOperation dvm_operation = lirInstrToDVMOperation(*this, lir_terminator);
	VISIT(dvm_operation, op, InstructionLowerer(this).lower(op));
}

DVMPlace compiler::backend_vm::internal::FunctionLoweringContext::pushTempLocal(
	const vm::code::TypeOfData& type, base::Optional<std::string_view> name_hint, bool tracked
) {
	auto name       = base::strConcat(name_hint.copyValueOr("temp"), next_temp_id++);
	auto temp_local = DVMPlace(base::StrID(name), type, DVMPlace::AccessKind::Direct);
	if (tracked) current_temp_count++;

	pushInstruction({
		OpKind::init,
		temp_local.asAnyArgument(),
		vm::opargs::Type(typeName(type)),
	});
	return temp_local;
}

void FunctionLoweringContext::cleanUpRegisteredTemps() {
	for (; current_temp_count > 0; current_temp_count--)
		pushInstruction({ vm::code::instructions::Op_deinit() });
}
