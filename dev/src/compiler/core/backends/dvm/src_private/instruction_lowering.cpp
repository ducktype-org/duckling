#include "dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "program_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"

#include "string_id/string_id.hpp"
#include <logger/logger.hpp>

#include "vm/bytecode/type_of_data.hpp"
#include <vm/bytecode/builders/instruction_builder.hpp>
#include <vm/bytecode/opcode_args.hpp>

#include <ranges>

using namespace compiler::backend_vm::internal;
using namespace vm::code;
using namespace compiler;
using namespace vm::code::builders;

namespace {
	vm::code::builders::OpKind lirOpToOpKind(lir::Operation operation) {
		switch (operation) {
		/// Integer operations ///
		case lir::Operation::IntegerAdd:
			return OpKind::add;
		case lir::Operation::IntegerSub:
			return OpKind::sub;
		case lir::Operation::IntegerNeg:
			return OpKind::neg;
		case lir::Operation::IntegerMul:
			return OpKind::mul;
		case lir::Operation::IntegerSDiv:
			return OpKind::div;
		case lir::Operation::IntegerSMod:
			return OpKind::mod;
		case lir::Operation::IntegerUDiv:
			return OpKind::udiv;
		case lir::Operation::IntegerUMod:
			return OpKind::umod;

		/// Floating point operations ///
		case lir::Operation::FloatAdd:
			return OpKind::fadd;
		case lir::Operation::FloatSub:
			return OpKind::fsub;
		case lir::Operation::FloatMul:
			return OpKind::fmul;
		case lir::Operation::FloatDiv:
			return OpKind::fdiv;
		case lir::Operation::FloatNeg:
			return OpKind::fneg;

		/// Signed integer comparisons ///
		case lir::Operation::IntegerEq:
			return OpKind::cmpEq;
		case lir::Operation::IntegerNeq:
			return OpKind::cmpNeq;
		case lir::Operation::IntegerSLt:
			return OpKind::cmpL;
		case lir::Operation::IntegerSLteq:
			return OpKind::cmpLe;
		case lir::Operation::IntegerSGt:
			return OpKind::cmpG;
		case lir::Operation::IntegerSGteq:
			return OpKind::cmpGe;

		/// Unsigned integer comparisons ///
		case lir::Operation::IntegerULt:
			return OpKind::ucmpL;
		case lir::Operation::IntegerULteq:
			return OpKind::ucmpLe;
		case lir::Operation::IntegerUGt:
			return OpKind::ucmpG;
		case lir::Operation::IntegerUGteq:
			return OpKind::ucmpGe;

		/// Floating point comparisons ///
		case lir::Operation::FloatLt:
			return OpKind::fcmpL;
		case lir::Operation::FloatGt:
			return OpKind::fcmpG;
		case lir::Operation::FloatLteq:
			return OpKind::fcmpLe;
		case lir::Operation::FloatGteq:
			return OpKind::fcmpGe;
		case lir::Operation::FloatEq:
			return OpKind::fcmpEq;
		case lir::Operation::FloatNeq:
			return OpKind::fcmpNeq;

		/// Logical operations ///
		case lir::Operation::BooleanAnd:
			return OpKind::log_and;
		case lir::Operation::BooleanOr:
			return OpKind::log_or;
		case lir::Operation::BooleanNot:
			return OpKind::log_not;

		/// Other ///
		case lir::Operation::Assign:
			return OpKind::mov;
		case lir::Operation::Call:
			return OpKind::call;

		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
		CORE_UNREACHABLE();
	}

	bool isComparison(OpKind op) {
		return op == OpKind::cmpEq || op == OpKind::cmpNeq || op == OpKind::cmpL
		    || op == OpKind::cmpLe || op == OpKind::cmpG || op == OpKind::cmpGe
		    || op == OpKind::ucmpL || op == OpKind::ucmpLe || op == OpKind::ucmpG
		    || op == OpKind::ucmpGe || op == OpKind::fcmpEq || op == OpKind::fcmpNeq
		    || op == OpKind::fcmpL || op == OpKind::fcmpLe || op == OpKind::fcmpG
		    || op == OpKind::fcmpGe;
	}

	bool isUnaryOperation(OpKind op) {
		return op == OpKind::neg || op == OpKind::fneg || op == OpKind::log_not;
	}

	bool isMetaTypeOperation(compiler::lir::Operation op) {
		return op == lir::Operation::MetaCreateBox || op == lir::Operation::MetaCreateRef
		    || op == lir::Operation::MetaCreateConst || op == lir::Operation::MetaCreateOptional
		    || op == lir::Operation::MetaCreateTuple || op == lir::Operation::MetaCreateVariant
		    || op == lir::Operation::MetaCreateFuncType || op == lir::Operation::MetaGetSize;
	}

	static u64 getByteSizeForType(base::StrID type_name) {
		if (type_name == "i64" || type_name == "f64" || type_name == "u64" || type_name == "ptr"
		    || type_name == "opaque_ptr")
			return 8;
		if (type_name == "i32" || type_name == "f32" || type_name == "u32") return 4;
		if (type_name == "i16" || type_name == "u16") return 2;
		if (type_name == "i8" || type_name == "u8" || type_name == "bool") return 1;
		CORE_PANIC("Unsupported VM type");
	}

	vm::code::TypeOfData getTypeFromSignatureName(base::StrID type_name) {
		if (type_name == "opaque_ptr")
			return vm::code::OpaqueType{ type_name, getByteSizeForType(type_name) };
		return vm::code::PrimitiveType(type_name, getByteSizeForType(type_name));
	}
}

// TODOP: Unify handleCall and handleExtCall
void FunctionLoweringContext::handleExtCall(
	const base::StrID&          func_name,
	const std::deque<DVMValue>& func_args,
	base::Optional<DVMValue>    output

) {
	const auto& extern_func_signature = program_context.getExternCFunction(func_name).signature;
	CORE_ASSERT(
		extern_func_signature.parameters.size() == func_args.size(),
		"Argument count mismatch for extern C function call: ",
		func_name
	);

	auto call_result_storage = [&] -> base::Optional<DVMLocal> {
		auto result_type_name = extern_func_signature.result_type.str;
		if (result_type_name != "void") {
			// TODOP: Would be nice for extern functions to store TypeOfData instead of identifiers
			auto arg_type = getTypeFromSignatureName(result_type_name);
			return pushTempLocal(arg_type, "call_result");
		} else
			return {};
	}();

	for (const auto& [arg_id, func_arg, type_name]:
	     std::views::zip(std::views::iota(0), func_args, extern_func_signature.parameters)) {
		auto arg_type = getTypeFromSignatureName(type_name.str);

		CORE_DEV_LOG(Backend, "Initializing: ", typeName(arg_type), '\n');
		auto arg_name = base::strConcat("ext_call", "_arg", arg_id, "_");
		std::cout << "Arg type in handleExtCall: " << typeName(arg_type).strView() << '\n';
		auto temp_arg = pushTempLocal(arg_type, arg_name.c_str());
		pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
	}

	pushInstruction(
		{ OpKind::call,
	      DVMFunctionName{ .name = base::StrID(func_name), .is_extern_c = true }.asArgument() }
	);

	if (output) {
		pushInstruction({
			OpKind::mov,
			output.value(),
			call_result_storage->asArgument(),
		});
	}

	if (call_result_storage) pushInstruction({ instructions::Op_deinit() });  // Deinit func result
}

void FunctionLoweringContext::handleFunctionCall(
	const lir::FunctionLiteral& called_function,
	const DVMValue&             called_func_name,
	const std::deque<DVMValue>& func_args,
	base::Optional<DVMValue>    output
) {
	CORE_ASSERT(
		func_args.size() == called_function.parameter_layouts->size(),
		"Function call argument count does not match function parameter count."
	);

	vm::code::TypeOfData called_result_type
		= program_context.lowerAndKeepTslType(called_function.return_type_layout);
	std::vector<vm::code::TypeOfData> param_types
		= *called_function.parameter_layouts | std::views::transform([&](const auto& layout) {
			  return program_context.lowerAndKeepTslType(layout);
		  })
	    | std::ranges::to<std::vector>();


	auto call_result_storage = [&] -> base::Optional<DVMLocal> {
		if (typeName(called_result_type) != "void")
			return pushTempLocal(called_result_type, "call_result");
		else
			return {};
	}();

	// Instantiate function parameters on the stack.
	for (const auto& [arg_id, func_arg, param_type]:
	     std::views::zip(std::views::iota(0), func_args, param_types)) {
		CORE_DEV_LOG(Backend, "Initializing: ", typeName(param_type), '\n');

		auto arg_name = base::strConcat("call", "_arg", arg_id, "_");

		auto temp_arg = pushTempLocal(param_type, arg_name.c_str());

		pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
	}

	pushInstruction({ OpKind::call, called_func_name });

	if (output) {
		pushInstruction({
			OpKind::mov,
			output.value(),
			call_result_storage.value().asArgument(),
		});
	}

	if (call_result_storage) pushInstruction({ instructions::Op_deinit() });  // Deinit func result
}

void FunctionLoweringContext::handleMetaOperation(const lir::Instruction& lir_instruction) {
	using namespace compiler::lir;

	std::deque<DVMValue> args
		= lir_instruction.arguments
	    | std::views::transform([&](const auto& lir_arg) { return lowerLirValue(lir_arg); })
	    | std::ranges::to<std::deque>();

	const auto maybe_output
		= lir_instruction.output.map([&](const auto& output) { return lowerLirValue(output); });

	// TODOP: Figure out what to do with the context.
	auto ctx_local = pushTempLocal(vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "ctx");
	pushInstruction({ OpKind::mov,
	                  ctx_local.asArgument(),
	                  vm::opargs::GlobalOpq(base::StrID("__comptime_query_ctx")) });
	DVMValue ctx = { ctx_local };
	// TODOP: Make those function names not hardcoded?
	switch (lir_instruction.operation) {
	case Operation::MetaCreateBox:
		std::cout << "Meta create box\n";
		handleExtCall(base::StrID("__comptime_create_box"), { args[0] }, maybe_output);
		break;
	case Operation::MetaCreateRef:
		handleExtCall(base::StrID("__comptime_create_ref"), { args[0] }, maybe_output);
		break;
	case Operation::MetaCreateOptional:
		handleExtCall(base::StrID("__comptime_create_optional"), { args[0] }, maybe_output);
		break;
	case Operation::MetaCreateConst:
		handleExtCall(base::StrID("__comptime_create_const"), { args[0] }, maybe_output);
		break;
	case Operation::MetaGetSize:
		handleExtCall(base::StrID("__comptime_get_size"), { ctx, args[0] }, maybe_output);
		break;
	case Operation::MetaCreateTuple: {
		auto builder
			= pushTempLocal(vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "tuple_builder");
		DVMValue builder_value = { DVMLocal{ .name = builder.name, .type = builder.type } };

		handleExtCall(base::StrID("__comptime_tuple_builder_new"), {}, builder_value);

		for (usize i = 0; i < lir_instruction.arguments.size(); i++) {
			handleExtCall(
				base::StrID("__comptime_tuple_builder_push"), { builder_value, args[i] }, {}
			);
		}

		handleExtCall(
			base::StrID("__comptime_tuple_builder_finalize"), { ctx, builder_value }, maybe_output
		);

		// TODOP: Deinit builder?
		break;
	}
	case Operation::MetaCreateVariant: {
		auto builder
			= pushTempLocal(vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "variant_builder");
		DVMValue builder_value = { DVMLocal{ .name = builder.name, .type = builder.type } };

		handleExtCall(base::StrID("__comptime_variant_builder_new"), {}, builder_value);

		for (usize i = 0; i < lir_instruction.arguments.size(); i++)
			handleExtCall(
				base::StrID("__comptime_variant_builder_push"), { builder_value, args[i] }, {}
			);

		handleExtCall(
			base::StrID("__comptime_variant_builder_finalize"), { ctx, builder_value }, maybe_output
		);

		// TODOP: Deinit builder?
		break;
	}
	case Operation::MetaCreateFuncType: {
		CORE_ASSERT(!lir_instruction.arguments.empty(), "FuncType must have at least a return type");

		auto builder
			= pushTempLocal(vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "function_builder");
		DVMValue builder_value = { DVMLocal{ .name = builder.name, .type = builder.type } };

		handleExtCall(base::StrID("__comptime_func_type_builder_new"), {}, builder_value);

		handleExtCall(
			base::StrID("__comptime_func_type_set_ret_type"), { builder_value, args[0] }, {}
		);

		for (usize i = 1; i < lir_instruction.arguments.size(); i++)
			handleExtCall(
				base::StrID("__comptime_func_type_builder_push_arg"), { builder_value, args[i] }, {}
			);

		handleExtCall(
			base::StrID("__comptime_func_type_builder_finalize"),
			{ ctx, builder_value },
			maybe_output
		);

		// TODOP: Deinit builder?
		break;
	}
	default:
		CORE_PANIC("Unknown meta operation");
	}
}

void FunctionLoweringContext::pushInstruction(const lir::Instruction& lir_instruction) {
	if (isMetaTypeOperation(lir_instruction.operation)) {
		handleMetaOperation(lir_instruction);
		return;
	}

	std::deque<DVMValue> args
		= lir_instruction.arguments
	    | std::views::transform([&](const auto& lir_arg) { return lowerLirValue(lir_arg); })
	    | std::ranges::to<std::deque>();

	// TODOP: Integrate with meta ops so this returns a variant.
	const auto operation = lirOpToOpKind(lir_instruction.operation);

	const auto maybe_output
		= lir_instruction.output.map([&](const auto& output) { return lowerLirValue(output); });

	if (isComparison(operation)) {
		CORE_ASSERT(args.size() == 2, "Invalid comparison argument count");
		// This resolves e.g. `x = a CMP b;`
		// by splitting it into two instructions:
		// a CMP b;
		// cmov x, 1;
		pushInstruction({ lirOpToOpKind(lir_instruction.operation), args[0], args[1] });
		pushInstruction({ OpKind::cmov, maybe_output.value(), DVMValue(1).asArgument() });
	} else if (operation == OpKind::call) {
		auto called_function  = lir_instruction.arguments.at(0).get<lir::FunctionLiteral>();
		auto called_func_name = args.front();
		args.pop_front();
		handleFunctionCall(called_function, called_func_name, args, maybe_output);
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
