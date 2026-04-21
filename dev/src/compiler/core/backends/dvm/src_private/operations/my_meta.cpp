#include "instruction_lowerer.hpp"


namespace {
	void lowerCreateBox(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_BOX), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void lowerCreateRef(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_REF), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void lowerCreateConst(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_CONST), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void lowerCreateTuple(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		lowerBuilderPattern(
			BuilderSequence{ .new_func  = base::StrID(comptime_func_names::TUPLE_BUILDER_NEW),
		                     .push_func = base::StrID(comptime_func_names::TUPLE_BUILDER_PUSH),
		                     .finalize_func
		                     = base::StrID(comptime_func_names::TUPLE_BUILDER_FINALIZE) },
			type_args,
			output
		);
	}

	void lowerCreateVariant(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		lowerBuilderPattern(
			BuilderSequence{ .new_func  = base::StrID(comptime_func_names::VARIANT_BUILDER_NEW),
		                     .push_func = base::StrID(comptime_func_names::VARIANT_BUILDER_PUSH),
		                     .finalize_func
		                     = base::StrID(comptime_func_names::VARIANT_BUILDER_FINALIZE) },
			type_args,
			output
		);
	}

	void lowerTypesEqual(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		CORE_ASSERT(type_args.size() == 2, "MetaEq should have two arguments");
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::TYPES_EQUAL), func_ctx.program_context
			),
			type_args,
			output
		);
	}

	void lowerTypesNotEqual(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		CORE_ASSERT(type_args.size() == 2, "MetaNeq should have two arguments");
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::TYPES_NOT_EQUAL), func_ctx.program_context
			),
			type_args,
			output
		);
	}

	void lowerBuilderPattern(
		const BuilderSequence&          builder_sequence,
		const std::deque<DVMValue>&     type_args,
		const base::Optional<DVMPlace>& output
	) {
		auto builder = func_ctx.pushTempLocal(
			vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "variant_builder"
		);
		DVMValue builder_value = { builder, DVMPlace::AccessKind::Direct };
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				builder_sequence.new_func, func_ctx.program_context
			),
			{},
			DVMPlace{ builder, DVMPlace::AccessKind::Direct }
		);

		for (const auto& type_arg: type_args) {
			func_ctx.handleCall(
				FunctionCallInfo::fromExternCFunction(
					builder_sequence.push_func, func_ctx.program_context
				),
				{ builder_value, type_arg },
				{}
			);
		}

		auto ctx = getQueryContext();
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				builder_sequence.finalize_func, func_ctx.program_context
			),
			{ ctx, builder_value },
			output
		);
	}

	DVMValue getQueryContext() {
		return DVMValue{ DVMGlobal{ .name = base::StrID(comptime_func_names::GLOBAL_QUERY_CONTEXT),
			                        .type = vm::code::OpaqueType(base::StrID("opaque_ptr"), 8) },
			             DVMPlace::AccessKind::Direct };
	}


}

namespace compiler::backend_vm::internal {

	// Normal instructions
	void InstructionLowerer::lower(const MetaOperation& op) {
		switch (op.meta_op) {
		case lir::Operation::MetaCreateBox:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateBox expects 1 argument");
			lowerCreateBox(op.args[0], op.dest);
			break;
		case lir::Operation::MetaCreateRef:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateRef expects 1 argument");
			lowerCreateRef(op.args[0], op.dest);
			break;
		case lir::Operation::MetaCreateConst:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateConst expects 1 argument");
			lowerCreateConst(op.args[0], op.dest);
			break;
		case lir::Operation::MetaCreateTuple:
			lowerCreateTuple(op.args, op.dest);
			break;
		case lir::Operation::MetaCreateVariant:
			lowerCreateVariant(op.args, op.dest);
			break;
		case lir::Operation::MetaEq:
			lowerTypesEqual(op.args, op.dest);
			break;
		case lir::Operation::MetaNeq:
			lowerTypesNotEqual(op.args, op.dest);
			break;
		default:
			CORE_PANIC("Unknown meta operation: ", base::enumToStr(op.meta_op));
		}
    }

}
