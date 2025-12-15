#include "meta_operation_lowering.hpp"

#include "dvm_operation.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"

#include "base/except/exceptions.hpp"
#include "base/str/str_utils.hpp"

#include "string_id/string_id.hpp"

namespace compiler::backend_vm::internal {

	void MetaOperationLowerer::lower(
		const MetaOperation&            meta_operation,
		const std::deque<DVMValue>&     args,
		const base::Optional<DVMValue>& output
	) {
		switch (meta_operation.meta_op) {
		case lir::Operation::MetaCreateBox:
			CORE_ASSERT(args.size() == 1, "MetaCreateBox expects 1 argument");
			lowerCreateBox(args[0], output);
			break;
		case lir::Operation::MetaCreateRef:
			CORE_ASSERT(args.size() == 1, "MetaCreateBox expects 1 argument");
			lowerCreateRef(args[0], output);
			break;
		case lir::Operation::MetaCreateTuple:
			lowerCreateTuple(args, output);
			break;
		case lir::Operation::MetaCreateVariant:
			lowerCreateVariant(args, output);
			break;
		default:
			CORE_PANIC("Unknown meta operation: ", base::enumToStr(meta_operation.meta_op));
		}
	}

	void MetaOperationLowerer::lowerCreateBox(
		const DVMValue& type_arg, const base::Optional<DVMValue>& output
	) {
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_BOX), func_ctx.getProgramContext()
			),
			{ type_arg },
			output
		);
	}

	void MetaOperationLowerer::lowerCreateRef(
		const DVMValue& type_arg, const base::Optional<DVMValue>& output
	) {
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				base::StrID(comptime_func_names::CREATE_REF), func_ctx.getProgramContext()
			),
			{ type_arg },
			output
		);
	}

	void MetaOperationLowerer::lowerCreateTuple(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMValue>& output
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

	void MetaOperationLowerer::lowerCreateVariant(
		const std::deque<DVMValue>& type_args, const base::Optional<DVMValue>& output
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

	void MetaOperationLowerer::lowerBuilderPattern(
		const BuilderSequence&          builder_sequence,
		const std::deque<DVMValue>&     type_args,
		const base::Optional<DVMValue>& output
	) {
		auto builder = func_ctx.pushTempLocal(
			vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "variant_builder"
		);
		DVMValue builder_value = { DVMLocal{ .name = builder.name, .type = builder.type } };
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				builder_sequence.new_func, func_ctx.getProgramContext()
			),
			{},
			builder_value
		);

		for (const auto& type_arg: type_args) {
			func_ctx.handleCall(
				FunctionCallInfo::fromExternCFunction(
					builder_sequence.push_func, func_ctx.getProgramContext()
				),
				{ builder_value, type_arg },
				{}
			);
		}

		auto ctx = getQueryContext();
		func_ctx.handleCall(
			FunctionCallInfo::fromExternCFunction(
				builder_sequence.finalize_func, func_ctx.getProgramContext()
			),
			{ ctx, builder_value },
			output
		);
		// TODOP: Deinit builders?
	}

	DVMValue MetaOperationLowerer::getQueryContext() {
		auto ctx_local
			= func_ctx.pushTempLocal(vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "ctx");

		func_ctx.pushInstruction(
			{ OpKind::mov,
		      ctx_local.asArgument(),
		      vm::opargs::GlobalOpq(base::StrID(comptime_func_names::GLOBAL_QUERY_CONTEXT)) }
		);

		return DVMValue{ ctx_local };
	}
}
