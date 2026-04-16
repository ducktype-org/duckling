#include "debug_info_utils.hpp"
#include "dvm_operation.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "operations/arithmetic_operation_lowering.hpp"
#include "operations/cast_operation_lowering.hpp"
#include "operations/comparison_operation_lowering.hpp"
#include "operations/meta_operation_lowering.hpp"

#include <lir/lir_structure/lir_structure.hpp>
#include <program_lowering_context.hpp>

#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"

#include <logger/logger.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

using namespace compiler::backend_vm::internal;
using namespace vm::code;
using namespace compiler;
using namespace vm::code::builders;

void FunctionLoweringContext::handleCall(
	const FunctionCallInfo&     call_info,
	const std::deque<DVMValue>& func_args,
	base::Optional<DVMPlace>    output
) {
	CORE_ASSERT(
		call_info.param_types.size() == func_args.size(),
		"Argument count mismatch for extern C function call: ",
		VISIT(call_info.call_target, callable, return callable.name)
	);

	auto call_result_storage = [&] -> base::Optional<DVMLocal> {
		if (call_info.return_type)
			return pushTempLocal(call_info.return_type.value(), "call_result");
		else
			return {};
	}();

	for (const auto& [arg_idx, func_arg, arg_type]:
	     std::views::zip(std::views::iota(0), func_args, call_info.param_types)) {
		CORE_DEV_LOG(Backend, "Initializing: ", typeName(arg_type), '\n');

		auto arg_name = base::strConcat("call", "_arg", arg_idx, "_");
		auto temp_arg = pushTempLocal(arg_type, arg_name, false);
		pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
	}

	pushInstruction({ OpKind::call,
	                  VISIT(call_info.call_target, callable, return callable.asArgument()) });

	if (output)
		storeResult(output.value(), { call_result_storage.value(), DVMPlace::AccessKind::Direct });
}

void FunctionLoweringContext::pushInstruction(const lir::Instruction& lir_instruction) {
	// Schedule cleaning of all temporaries created by `pushTempLocal` while lowering this instruction.
	defer(cleanUpRegisteredTemps());

	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_instruction.metadata.position, pos) {
			builder.addInstruction(instructionsCount(), mapDIPosition(pos));
		}
	}

	// TODOP: All of the outputs should be optional.
	auto dvm_operation = lirInstrToDVMOperation(*this, lir_instruction);
	variant_match(dvm_operation) {
		variant_case(MetaOperation, operation) {
			MetaOperationLowerer(*this).lower(operation);
			return;
		}
		variant_case(CastOperation, operation) {
			CastOperationLowerer::lowerCastOperation(*this, operation);
			return;
		}
		variant_case(ComparisonOperation, operation) {
			ComparisonOperationLowerer::lower(*this, operation);
			return;
		}
		variant_case(UnaryOperation, operation) {
			ArithmeticOperationLowerer::lowerUnary(*this, operation);
			return;
		}
		variant_case(BinaryOperation, operation) {
			ArithmeticOperationLowerer::lowerBinary(*this, operation);
			return;
		}
		variant_case(CallOperation, operation) {
			handleCall(operation.call_info, operation.args, operation.dest);
			return;
		}
		variant_case(AddressOfOperation, operation) {
			DVMPlace resolved_src = operation.src;
			DVMPlace output_dest  = operation.dest;

			auto addr_temp = pushTempLocal(
				program_context.lowerAndKeepTslType(lir_instruction.output->layout), "addr_of"
			);

			if (resolved_src.isDirect()) {
				// If access to the variable is direct, we take it's address.
				pushInstruction({ OpKind::ref, addr_temp.asArgument(), resolved_src.asAnyArgument() }
				);
			} else {
				// Otherwise, if the resolved source is accessed through a pointer
				// (AccessKind::Pointer), than we have the address in hand. We just move it.
				pushInstruction({ OpKind::mov, addr_temp.asArgument(), resolved_src.asArgument() });
			}
			storeResult(output_dest, { addr_temp, DVMPlace::AccessKind::Direct });
			return;
		}
		variant_case(MoveOperation, operation) {
			// TODOP: Move this somewhere.
			// Otherwise, it's a simple assignment.
			storeResult(operation.dest, operation.src);
			return;
		}
		variant_case_novalue(NoOpOperation) { return; }
		variant_default { CORE_UNREACHABLE(); }
	}
}

// TODOP: Mov to TerminatorOperationLowering
void FunctionLoweringContext::pushTerminator(const lir::Instruction& lir_terminator) {
	pushInstruction(instructions::Comment(base::StrID(
		base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
	)));

	if_opt_some(fun_di_builder_opt, builder) {
		if_opt_some(lir_terminator.metadata.position, pos) {
			builder.addInstruction(instructionsCount(), mapDIPosition(pos));
		}
	}

	if (lir_terminator.operation == lir::Operation::Branch) {
		auto bool_arg    = lowerLirValue(lir_terminator.arguments.at(0));
		auto true_block  = lowerLirValue(lir_terminator.arguments.at(1));
		auto false_block = lowerLirValue(lir_terminator.arguments.at(2));


		variant_match(lir_terminator.arguments.at(0).getVariant()) {
			variant_case(lir::LIRConstant, constant) {
				auto bool_val
					= constant.value.get<bool>().expect("Expected boolean in LIRConstant");
				cleanUpRegisteredTemps();
				if (bool_val)
					pushInstruction({ OpKind::jmp, true_block });
				else
					pushInstruction({ OpKind::jmp, false_block });
			}
			variant_default {
				pushInstruction({ OpKind::cmpEq, bool_arg, vm::opargs::Immediate{ 1 } });
				cleanUpRegisteredTemps();
				pushInstruction({ OpKind::jmpIf, true_block });
				pushInstruction({ OpKind::jmpIfNot, false_block });
			}
		}
	} else if (lir_terminator.operation == lir::Operation::ReturnVoid) {
		pushInstruction({ OpKind::ret });
	} else if (lir_terminator.operation == lir::Operation::Jump) {
		CORE_ASSERT(
			lir_terminator.arguments.size() == 1, "Invalid number of arguments for jump terminator."
		);
		auto target_block = lowerLirValue(lir_terminator.arguments.at(0));
		pushInstruction({ OpKind::jmp, target_block });
	} else if (lir_terminator.operation == lir::Operation::ReturnValue) {
		CORE_ASSERT(
			lir_terminator.arguments.size() == 1, "Invalid number of arguments for value-return."
		);

		// Since VM does not support `return X;` operation, we must move the value to
		// the ret_val local and then return.
		pushInstruction({
			OpKind::mov,
			getFunctionReturnValueLocal().asArgument(),
			lowerLirValue(lir_terminator.arguments.at(0)),
		});
		cleanUpRegisteredTemps();
		pushInstruction({ OpKind::ret });
	} else {
		CORE_PANIC("Invalid terminator: ", base::enumToStr(lir_terminator.operation));
	}
}

DVMLocal compiler::backend_vm::internal::FunctionLoweringContext::pushTempLocal(
	const vm::code::TypeOfData& type, base::Optional<std::string_view> name_hint, bool tracked
) {
	auto name       = base::strConcat(name_hint.copyValueOr("temp"), next_temp_id++);
	auto temp_local = DVMLocal{
		.name = base::StrID(name),
		.type = type,
	};
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
