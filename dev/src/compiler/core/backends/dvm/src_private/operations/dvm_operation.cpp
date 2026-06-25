#include "dvm_operation.hpp"

#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"

#include <ctv/ctv.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/optional.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <ranges>

namespace {
	using namespace compiler;

	bool isMetaTypeOperation(lir::Operation op) {
		return op == lir::Operation::MetaCreateBox || op == lir::Operation::MetaCreateRef
		    || op == lir::Operation::MetaCreateConst || op == lir::Operation::MetaCreateTuple
		    || op == lir::Operation::MetaCreateVariant || op == lir::Operation::MetaEq
		    || op == lir::Operation::MetaNeq;
	}

	vm::code::builders::OpKind lirOperationToDVMOpKind(const lir::Operation& op) {
		using enum lir::Operation;
		using namespace vm::code::builders;
		// clang-format off
		switch (op) {
		case IntegerNeg: 	return OpKind::neg;
		case FloatNeg:   	return OpKind::fneg;
		case BooleanNot: 	return OpKind::log_not;
		case IntegerAdd:  	return OpKind::add;
		case IntegerSub:  	return OpKind::sub;
		case IntegerMul:  	return OpKind::mul;
		case IntegerSDiv: 	return OpKind::div;
		case IntegerSMod: 	return OpKind::mod;
		case IntegerUDiv: 	return OpKind::udiv;
		case IntegerUMod: 	return OpKind::umod;
		case FloatAdd:    	return OpKind::fadd;
		case FloatSub:    	return OpKind::fsub;
		case FloatMul:    	return OpKind::fmul;
		case FloatDiv:    	return OpKind::fdiv;
		case BooleanAnd:  	return OpKind::log_and;
		case BooleanOr:   	return OpKind::log_or;
		case IntegerEq:    	return OpKind::cmpEq;
		case IntegerNeq:   	return OpKind::cmpNeq;
		case IntegerSLt:   	return OpKind::cmpLt;
		case IntegerSLteq: 	return OpKind::cmpLe;
		case IntegerSGt:   	return OpKind::cmpGt;
		case IntegerSGteq: 	return OpKind::cmpGe;
		case IntegerULt:   	return OpKind::ucmpLt;
		case IntegerULteq: 	return OpKind::ucmpLe;
		case IntegerUGt:   	return OpKind::ucmpGt;
		case IntegerUGteq: 	return OpKind::ucmpGe;
		case FloatLt:      	return OpKind::fcmpLt;
		case FloatGt:      	return OpKind::fcmpGt;
		case FloatLteq:    	return OpKind::fcmpLe;
		case FloatGteq:    	return OpKind::fcmpGe;
		case FloatEq:      	return OpKind::fcmpEq;
		case FloatNeq:     	return OpKind::fcmpNeq;
		default:         	CORE_UNREACHABLE();
		}
		// clang-format on
	}
}

namespace compiler::backend_vm::internal {
	FunctionCallInfo FunctionCallInfo::fromLirFunction(
		const lir::FunctionLiteral& func_literal, ProgramLoweringContext& program_context
	) {
		base::Optional<vm::code::TypeOfData> called_result_type
			= program_context.lowerAndKeepTslType(func_literal.return_type_layout)
		          .map([](CRef<vm::code::TypeOfData> ref) { return *ref; });

		std::vector<vm::code::TypeOfData> param_types
			= *func_literal.parameter_layouts | std::views::transform([&](const auto& layout) {
				  return **program_context.lowerAndKeepTslType(layout);
			  })
		    | std::ranges::to<std::vector>();

		return FunctionCallInfo{
			.call_target = DVMFunctionName{ .name = func_literal.mangled_name },
			.return_type = called_result_type,
			.param_types = param_types,
			.is_extern_c = false,
		};
	}

	FunctionCallInfo FunctionCallInfo::fromExternCFunction(
		const base::StrID& ext_func_name, ProgramLoweringContext& program_context
	) {
		const auto& ext_func = program_context.getExternCFunction(ext_func_name);

		CORE_ASSERT(
			ext_func.signature.result_types.size() <= 1, "Functions should return one value at most"
		);

		base::Optional<vm::code::TypeOfData> called_result_type = {};
		if (ext_func.signature.result_types.size()) {
			auto reslt         = ext_func.signature.result_types.at(0);
			called_result_type = vm::code::getBuiltinTypeByName(reslt).value();
		}

		std::vector<vm::code::TypeOfData> param_types
			= ext_func.signature.parameters | std::views::transform([&](const auto& type_name) {
				  return vm::code::getBuiltinTypeByName(type_name).value();
			  })
		    | std::ranges::to<std::vector>();

		return FunctionCallInfo{
			.call_target = DVMExternCFunctionName{ .name = ext_func_name },
			.return_type = called_result_type,
			.param_types = param_types,
			.is_extern_c = true,
		};
	}

	DVMOperation lirInstrToDVMOperation(FunctionLoweringContext& ctx, const lir::Instruction& instr) {
		using enum lir::Operation;
		auto operation = instr.operation;

		auto lower_all_args = [&]() -> std::deque<DVMValue> {
			return instr.arguments | std::views::transform([&](const auto& lir_arg) {
					   return ctx.lowerLirValue(lir_arg);
				   })
			     | std::ranges::to<std::deque>();
		};

		auto lower_arg  = [&](const lir::LIRValue& value) { return ctx.lowerLirValue(value); };
		auto lower_dest = [&]() -> DVMPlace {
			CORE_ASSERT(
				instr.output.has_value(),
				"Expected output place for operation: ",
				base::enumToStr(operation)
			);
			return ctx.resolveLirPlace(*instr.output);
		};
		auto lower_opt_dest = [&]() -> base::Optional<DVMPlace> {
			return instr.output.map([&](const lir::LIRPlace& place) {
				return ctx.resolveLirPlace(place);
			});
		};
		auto get_opt_ctv = [](const lir::LIRValue& value) -> base::Optional<ctv::CompileTimeValue> {
			if (value.is<lir::LIRConstant>()) return value.get<lir::LIRConstant>().value;
			return {};
		};

		if (isMetaTypeOperation(operation))
			return MetaOperation{
				.meta_op = operation,
				.args    = lower_all_args(),
				.dest    = lower_opt_dest(),
			};


		switch (operation) {
		/// Special operations ///
		case ZeroInitialize:
			// Data in DVM is zeroinitialized by default, so this is a NoOp.
			return NoOperation{};
		case Cast: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"Cast expects 1 arguments, but got: ",
				instr.arguments.size()
			);
			const auto cast_params = std::get_if<lir::CastParameters>(&instr.extra_params);
			CORE_ASSERT(cast_params != nullptr, "Cast instruction without parameters");
			return CastOperation{
				.cast_params = *cast_params,
				.src         = lower_arg(instr.arguments[0]),
				.dest        = lower_opt_dest(),
			};
		}
		case Call: {
			CORE_ASSERT(!instr.arguments.empty(), "Call expects at least 1 argument (the callable)");
			auto func_literal = instr.arguments[0].get<lir::FunctionLiteral>();
			auto dvm_call_info
				= FunctionCallInfo::fromLirFunction(func_literal, ctx.program_context);

			auto call_args = instr.arguments | std::views::drop(1)  // Drop the FunctionLiteral
			               | std::views::transform([&](const auto& lir_arg) {
								 return ctx.lowerLirValue(lir_arg);
							 })
			               | std::ranges::to<std::deque>();

			return CallOperation{
				.call_info = dvm_call_info,
				.args      = std::move(call_args),
				.dest      = lower_opt_dest(),
			};
		}
		case AddressOf: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"AddressOf expects one argument, got: ",
				instr.arguments.size()
			);
			CORE_ASSERT(
				instr.arguments[0].is<lir::LIRPlace>(), "AddressOf argument must be a LIRPlace"
			);

			// This is an edge case where LIRValues should not be lowered to DVMValue as this
			// creates a copy of the value we try to reference on the stack. We have to lower it to
			// a place and if it's direct, take a pointer to it, but if it's not, the resulting
			// address is the pointer returned by `resolveLirPlace`.
			return AddressOfOperation{
				.src  = ctx.resolveLirPlace(instr.arguments[0].get<lir::LIRPlace>()),
				.dest = lower_opt_dest(),
			};
		}
		case Assign: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"Assign expects 1 argument, got: ",
				instr.arguments.size()
			);
			return MoveOperation{
				.src  = lower_arg(instr.arguments[0]),
				.dest = lower_dest(),
			};
		}

		/// Unary operations ///
		case IntegerNeg:
		case FloatNeg:
		case BooleanNot: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"Unary operation expects 1 argument, got: ",
				instr.arguments.size()
			);
			return UnaryOperation{
				.op   = lirOperationToDVMOpKind(operation),
				.src  = lower_arg(instr.arguments[0]),
				.dest = lower_opt_dest(),
			};
		}

		/// Binary operations ///
		case IntegerAdd:
		case IntegerSub:
		case IntegerMul:
		case IntegerSDiv:
		case IntegerSMod:
		case IntegerUDiv:
		case IntegerUMod:
		case FloatAdd:
		case FloatSub:
		case FloatMul:
		case FloatDiv:
		case BooleanAnd:
		case BooleanOr: {
			CORE_ASSERT(
				instr.arguments.size() == 2,
				"Binary operation expects 2 arguments, got: ",
				instr.arguments.size()
			);
			return BinaryOperation{
				.op   = lirOperationToDVMOpKind(operation),
				.lhs  = lower_arg(instr.arguments[0]),
				.rhs  = lower_arg(instr.arguments[1]),
				.dest = lower_opt_dest(),
			};
		}

		/// Comparison operations ///
		case IntegerEq:
		case IntegerNeq:
		case IntegerSLt:
		case IntegerSLteq:
		case IntegerSGt:
		case IntegerSGteq:
		case IntegerULt:
		case IntegerULteq:
		case IntegerUGt:
		case IntegerUGteq:
		case FloatLt:
		case FloatGt:
		case FloatLteq:
		case FloatGteq:
		case FloatEq:
		case FloatNeq: {
			CORE_ASSERT(
				instr.arguments.size() == 2,
				"Comparison operation expects 2 arguments, got: ",
				instr.arguments.size()
			);
			return ComparisonOperation{
				.op        = lirOperationToDVMOpKind(operation),
				.lhs       = lower_arg(instr.arguments[0]),
				.rhs       = lower_arg(instr.arguments[1]),
				.dest      = lower_opt_dest(),
				.lhs_const = get_opt_ctv(instr.arguments[0]),
				.rhs_const = get_opt_ctv(instr.arguments[1]),
			};
		}

		/// Terminator operations ///
		case Jump: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"Jump operation expects 1 argument, got: ",
				instr.arguments.size()
			);
			return JumpOperation{
				.target      = lower_arg(instr.arguments[0]).get<DVMLabel>(),
				.scope_flags = instr.scope_flags,
			};
		}
		case Branch: {
			CORE_ASSERT(
				instr.arguments.size() == 3,
				"Branch operation expects 3 arguments, got: ",
				instr.arguments.size()
			);
			return BranchOperation{
				.condition    = lower_arg(instr.arguments[0]),
				.true_target  = lower_arg(instr.arguments[1]).get<DVMLabel>(),
				.false_target = lower_arg(instr.arguments[2]).get<DVMLabel>(),
				.scope_flags  = instr.scope_flags,
			};
		}
		case ReturnValue:
		case ReturnVoid: {
			CORE_ASSERT(
				instr.arguments.size() <= 1,
				"ReturnValue expects at most 1 argument, got: ",
				instr.arguments.size()
			);

			return ReturnOperation{
				.value       = instr.arguments.size() == 1 ? lower_arg(instr.arguments[0])
				                                           : base::Optional<DVMValue>(),
				.scope_flags = instr.scope_flags,
			};
		}
		case BoxAlloc: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"BoxAlloc operation expects 1 argument, got: ",
				instr.arguments.size()
			);
			return BoxAllocOperation{
				.src  = lower_arg(instr.arguments[0]),
				.dest = lower_opt_dest(),
			};
		}
		case BoxFree: {
			CORE_ASSERT(
				instr.arguments.size() == 1,
				"BoxFree operation expects 1 argument, got: ",
				instr.arguments.size()
			);
			return BoxFreeOperation{
				.src = lower_arg(instr.arguments[0]),
			};
		}
		case Nop: {
			// No instruction to generate, just skip.
			return NoOperation{};
		}
		default:
			CORE_PANIC("Invalid operation: ", base::enumToStr(operation));
		}
	}
}
