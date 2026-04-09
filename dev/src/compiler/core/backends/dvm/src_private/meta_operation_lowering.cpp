#include "meta_operation_lowering.hpp"

#include "function_lowering_context.hpp"

namespace compiler::backend_vm::internal {

	void MetaOperationLowerer::lower(
		const MetaOperation&            meta_operation,
		const std::deque<DVMValue>&     args,
		const base::Optional<DVMPlace>& output
	) {
		switch (meta_operation.meta_op) {
		case lir::Operation::MetaCreateBox:
			CORE_ASSERT(args.size() == 1, "MetaCreateBox expects 1 argument");
			lowerCreateBox(args[0], output);
			break;
		case lir::Operation::MetaCreateRef:
			CORE_ASSERT(args.size() == 1, "MetaCreateRef expects 1 argument");
			lowerCreateRef(args[0], output);
			break;
		case lir::Operation::MetaCreateConst:
			CORE_ASSERT(args.size() == 1, "MetaCreateConst expects 1 argument");
			lowerCreateConst(args[0], output);
			break;
		case lir::Operation::MetaCreateTuple:
			lowerCreateTuple(args, output);
			break;
		case lir::Operation::MetaCreateVariant:
			lowerCreateVariant(args, output);
			break;
		case lir::Operation::MetaEq:
			lowerTypesEqual(args, output);
			break;
		case lir::Operation::MetaNeq:
			lowerTypesNotEqual(args, output);
			break;
		default:
			CORE_PANIC("Unknown meta operation: ", base::enumToStr(meta_operation.meta_op));
		}
	}

	void MetaOperationLowerer::lowerCreateBox(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_BOX), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void MetaOperationLowerer::lowerCreateRef(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_REF), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void MetaOperationLowerer::lowerCreateConst(
		const DVMValue& type_arg, const base::Optional<DVMPlace>& output
	) {
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_CONST), func_ctx.program_context
			),
			{ type_arg },
			output
		);
	}

	void MetaOperationLowerer::lowerCreateTuple(
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

	void MetaOperationLowerer::lowerCreateVariant(
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

	void MetaOperationLowerer::lowerTypesEqual(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		CORE_ASSERT(type_args.size() == 2, "MetaEq should have two arguments");
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::TYPES_EQUAL), func_ctx.program_context
			),
			type_args,
			output
		);
	}

	void MetaOperationLowerer::lowerTypesNotEqual(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
	) {
		CORE_ASSERT(type_args.size() == 2, "MetaNeq should have two arguments");
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::TYPES_NOT_EQUAL), func_ctx.program_context
			),
			type_args,
			output
		);
	}

	void MetaOperationLowerer::lowerBuilderPattern(
		const BuilderSequence&          builder_sequence,
		const std::deque<DVMValue>&     type_args,
		const base::Optional<DVMPlace>& output
	) {
		auto builder = func_ctx.pushTempLocal(
			vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "variant_builder"
		);
		DVMValue builder_value = { builder, DVMPlace::AccessKind::Direct };
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				builder_sequence.new_func, func_ctx.program_context
			),
			{},
			DVMPlace{ builder, DVMPlace::AccessKind::Direct }
		);

		for (const auto& type_arg: type_args) {
			func_ctx.handleCall(
				FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
					builder_sequence.push_func, func_ctx.program_context
				),
				{ builder_value, type_arg },
				{}
			);
		}

		auto ctx = getQueryContext();
		func_ctx.handleCall(
			FunctionLoweringContext::FunctionCallInfo::fromExternCFunction(
				builder_sequence.finalize_func, func_ctx.program_context
			),
			{ ctx, builder_value },
			output
		);
	}

	DVMValue MetaOperationLowerer::getQueryContext() {
		return DVMValue{ DVMGlobal{ .name = base::StrID(comptime_func_names::GLOBAL_QUERY_CONTEXT),
			                        .type = vm::code::OpaqueType(base::StrID("opaque_ptr"), 8) },
			             DVMPlace::AccessKind::Direct };
	}
}
