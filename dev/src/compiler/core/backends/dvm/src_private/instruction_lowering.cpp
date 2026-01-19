#include "cast_operation_lowering.hpp"
#include "dvm_operation.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "meta_operation_lowering.hpp"
#include "program_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <logger/logger.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

using namespace compiler::backend_vm::internal;
using namespace vm::code;
using namespace compiler;
using namespace vm::code::builders;

namespace {
	bool isComparison(OpKind op) {
		return op == OpKind::cmpEq || op == OpKind::cmpNeq || op == OpKind::cmpLt
		    || op == OpKind::cmpLe || op == OpKind::cmpGt || op == OpKind::cmpGe
		    || op == OpKind::ucmpLt || op == OpKind::ucmpLe || op == OpKind::ucmpGt
		    || op == OpKind::ucmpGe || op == OpKind::fcmpEq || op == OpKind::fcmpNeq
		    || op == OpKind::fcmpLt || op == OpKind::fcmpLe || op == OpKind::fcmpGt
		    || op == OpKind::fcmpGe;
	}

	bool isUnaryOperation(OpKind op) {
		return op == OpKind::neg || op == OpKind::fneg || op == OpKind::log_not;
	}
}

void FunctionLoweringContext::handleCall(
	const FunctionCallInfo&     call_info,
	const std::deque<DVMValue>& func_args,
	base::Optional<DVMValue>    output
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
		auto temp_arg = pushTempLocal(arg_type, arg_name.c_str());
		pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
	}

	pushInstruction({ OpKind::call,
	                  VISIT(call_info.call_target, callable, return callable.asArgument()) });

	if (output) {
		pushInstruction({
			OpKind::mov,
			output.value(),
			call_result_storage->asArgument(),
		});
	}

	if (call_result_storage) pushInstruction({ instructions::Op_deinit() });  // Deinit func result
}

void FunctionLoweringContext::pushInstruction(const lir::Instruction& lir_instruction) {
	std::deque<DVMValue> args
		= lir_instruction.arguments
	    | std::views::transform([&](const auto& lir_arg) { return lowerLirValue(lir_arg); })
	    | std::ranges::to<std::deque>();
	const auto maybe_output
		= lir_instruction.output.map([&](const auto& output) { return lowerLirValue(output); });

	const auto dvm_operation = lirInstrToDVMOperation(lir_instruction);

	variant_match(dvm_operation) {
		variant_case(MetaOperation, operation) {
			MetaOperationLowerer lowerer(*this);
			lowerer.lower(operation, args, maybe_output);
			return;
		}
		variant_case(CastOperation, operation) {
			CastOperationLowerer::lowerCastOperation(operation, args, maybe_output, *this);
			return;
		}
	}
	const auto operation = std::get<SimpleOperation>(dvm_operation).op;


	if (isComparison(operation)) {
		CORE_ASSERT(args.size() == 2, "Invalid comparison argument count");
		CORE_ASSERT(
			maybe_output.has_value(), "Comparison operations must have an output destination"
		);
		auto output = maybe_output.value();

		// If the first argument is an immediate, we need to move it to a temporary local.
		// @TODO: #1848 Idealy this could be removed after the DVM supports comparisons between
		// immediates.
		base::Optional<DVMLocal> temp_lhs;
		if (args[0].is<DVMImmediate>()) {
			auto vm_type = program_context.lowerAndKeepTslType(
				lir_instruction.arguments[0].get<lir::LIRConstant>().layout
			);
			temp_lhs = pushTempLocal(vm_type, "cmp_lhs_tmp");
			pushInstruction({ OpKind::mov, temp_lhs->asArgument(), args[0] });
			args[0] = DVMValue(*temp_lhs);
		}

		// This resolves e.g. `x = a CMP b;`
		// by splitting it into three instructions:
		// a CMP b;
		// mov x, 0;
		// cmov x, 1;
		pushInstruction({ operation, args[0], args[1] });
		pushInstruction({ OpKind::mov, output, DVMImmediate(0).asArgument() });
		pushInstruction({ OpKind::cmov, output, DVMValue(1).asArgument() });

		// @TODO: #1848 same as above
		if (temp_lhs) pushInstruction({ instructions::Op_deinit() });

	} else if (operation == OpKind::call) {
		auto called_function  = lir_instruction.arguments.at(0).get<lir::FunctionLiteral>();
		auto called_func_name = args.front();
		args.pop_front();
		handleCall(
			FunctionCallInfo::fromLirFunction(called_function, program_context), args, maybe_output
		);
	} else if (isUnaryOperation(operation)) {
		CORE_ASSERT(args.size() == 1, "Invalid unary operation argument count");
		auto output = maybe_output.value();
		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;
		if (output != args[0]) pushInstruction({ OpKind::mov, output, args[0] });

		pushInstruction({ operation, output });
	} else if (args.size() == 2) {
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;
		auto output = maybe_output.value();

		// If instruction is of the form: a = b OP c, then
		// we transform it to:
		// a = b;
		// a = a OP c;
		if (output != args[0]) pushInstruction({ OpKind::mov, output, args[0] });

		pushInstruction({ operation, output, args[1] });
	} else {
		auto output = maybe_output.value();
		pushInstruction({ operation, output, args[0] });
	}
}

void FunctionLoweringContext::pushTerminator(const lir::Instruction& lir_terminator) {
	pushInstruction(instructions::Comment(base::StrID(
		base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
	)));

	if (lir_terminator.operation == lir::Operation::Branch) {
		auto bool_arg    = lowerLirValue(lir_terminator.arguments.at(0));
		auto true_block  = lowerLirValue(lir_terminator.arguments.at(1));
		auto false_block = lowerLirValue(lir_terminator.arguments.at(2));

		variant_match(lir_terminator.arguments.at(0).getVariant()) {
			variant_case(lir::LIRConstant, constant) {
				auto bool_val
					= constant.value.get<bool>().expect("Expected boolean in LIRConstant");
				if (bool_val)
					pushInstruction({ OpKind::jmp, true_block });
				else
					pushInstruction({ OpKind::jmp, false_block });
			}
			variant_default {
				pushInstruction({ OpKind::cmpEq, bool_arg, vm::opargs::Immediate{ 1 } });
				pushInstruction({ OpKind::jmpIf, true_block });
				pushInstruction({ OpKind::jmpIfNot, false_block });
			}
		}
	}

	else if (lir_terminator.operation == lir::Operation::ReturnVoid) {
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
		pushInstruction({ OpKind::ret });
	} else {
		CORE_PANIC("Invalid terminator: ", base::enumToStr(lir_terminator.operation));
	}
}

DVMLocal compiler::backend_vm::internal::FunctionLoweringContext::pushTempLocal(
	const vm::code::TypeOfData& type, base::Optional<const char*> name_hint
) {
	auto name       = base::strConcat(name_hint.copyValueOr("temp"), next_temp_id++);
	auto temp_local = DVMLocal{
		.name = base::StrID(name.c_str()),
		.type = type,
	};
	pushInstruction({
		OpKind::init,
		vm::opargs::StackLocalAny(temp_local.name),
		vm::opargs::Type(typeName(type)),
	});
	return temp_local;
}
