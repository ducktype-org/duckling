#include "../dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "instruction_lowerer.hpp"
#include "operations/dvm_operation.hpp"

#include "base/collections/optional.hpp"

#include "string_id/string_id.hpp"

#include <deque>
#include <string_view>

/**
 * @brief Names of comptime type operation functions in DVM.
 */
namespace comptime_func_names {
	constexpr auto GLOBAL_QUERY_CONTEXT     = "comptime_query_ctx";
	constexpr auto CREATE_BOX               = "comptime_create_box";
	constexpr auto CREATE_REF               = "comptime_create_ref";
	constexpr auto CREATE_CONST             = "comptime_create_const";
	constexpr auto TYPES_EQUAL              = "comptime_types_equal";
	constexpr auto TYPES_NOT_EQUAL          = "comptime_types_not_equal";
	constexpr auto TUPLE_BUILDER_NEW        = "comptime_tuple_builder_new";
	constexpr auto TUPLE_BUILDER_PUSH       = "comptime_tuple_builder_push";
	constexpr auto TUPLE_BUILDER_FINALIZE   = "comptime_tuple_builder_finalize";
	constexpr auto VARIANT_BUILDER_NEW      = "comptime_variant_builder_new";
	constexpr auto VARIANT_BUILDER_PUSH     = "comptime_variant_builder_push";
	constexpr auto VARIANT_BUILDER_FINALIZE = "comptime_variant_builder_finalize";
}

namespace {
	using namespace compiler::backend_vm::internal;

	/**
	 * @brief POD struct storing all information for generating builder patterns for complex types
	 * (like variants or tuples).
	 */
	struct BuilderSequence {
		base::StrID new_func;
		base::StrID push_func;
		base::StrID finalize_func;
	};

	/**
	 * @brief Initializes a local variable of type `opaque_ptr` and stores the global query
	 * context value in it. Returns the created DVMValue.
	 */
	DVMValue getQueryContext() {
		return DVMValue{ DVMGlobal{ .name = base::StrID(comptime_func_names::GLOBAL_QUERY_CONTEXT),
			                        .type = vm::code::OpaqueType(base::StrID("opaque_ptr"), 8) },
			             DVMPlace::AccessKind::Direct };
	}

	void lowerSingleCall(
		Ref<InstructionLowerer>         lowerer,
		Ref<FunctionLoweringContext>    func_ctx,
		std::string_view                func_name,
		const std::deque<DVMValue>&     args,
		const base::Optional<DVMPlace>& output
	) {
		CallOperation call_op{
			.call_info = FunctionCallInfo::fromExternCFunction(
				base::StrID(func_name), func_ctx->program_context
			),
			.args = args,
			.dest = output,

		};
		lowerer->lower(call_op);
	}

	void lowerBuilderPattern(
		Ref<InstructionLowerer>         lowerer,
		Ref<FunctionLoweringContext>    func_ctx,
		const BuilderSequence&          builder_sequence,
		const std::deque<DVMValue>&     type_args,
		const base::Optional<DVMPlace>& output
	) {
		auto builder = func_ctx->pushTempLocal(
			vm::code::OpaqueType(base::StrID("opaque_ptr"), 8), "meta_builder"
		);
		DVMPlace builder_place = { builder, DVMPlace::AccessKind::Direct };

		lowerSingleCall(
			lowerer, func_ctx, builder_sequence.new_func.str(), std::deque<DVMValue>(), builder_place
		);

		DVMValue builder_value = { builder, DVMPlace::AccessKind::Direct };
		for (const auto& type_arg: type_args) {
			lowerSingleCall(
				lowerer, func_ctx, builder_sequence.push_func.str(), { builder_value, type_arg }, {}
			);
		}

		auto query_ctx_val = getQueryContext();
		lowerSingleCall(
			lowerer,
			func_ctx,
			builder_sequence.finalize_func.str(),
			{ query_ctx_val, builder_value },
			output
		);
	}
}

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const MetaOperation& op) {
		switch (op.meta_op) {
		case lir::Operation::MetaCreateBox:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateBox expects 1 argument");
			lowerSingleCall(this, ctx, comptime_func_names::CREATE_BOX, op.args, op.dest);
			break;
		case lir::Operation::MetaCreateRef:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateRef expects 1 argument");
			lowerSingleCall(this, ctx, comptime_func_names::CREATE_REF, op.args, op.dest);
			break;
		case lir::Operation::MetaCreateConst:
			CORE_ASSERT(op.args.size() == 1, "MetaCreateConst expects 1 argument");
			lowerSingleCall(this, ctx, comptime_func_names::CREATE_CONST, op.args, op.dest);
			break;
		case lir::Operation::MetaCreateTuple:
			lowerBuilderPattern(
				*this,
				ctx,
				BuilderSequence{ .new_func  = base::StrID(comptime_func_names::TUPLE_BUILDER_NEW),
			                     .push_func = base::StrID(comptime_func_names::TUPLE_BUILDER_PUSH),
			                     .finalize_func
			                     = base::StrID(comptime_func_names::TUPLE_BUILDER_FINALIZE) },
				op.args,
				op.dest
			);
			break;
		case lir::Operation::MetaCreateVariant:
			lowerBuilderPattern(
				*this,
				ctx,
				BuilderSequence{
					.new_func      = base::StrID(comptime_func_names::VARIANT_BUILDER_NEW),
					.push_func     = base::StrID(comptime_func_names::VARIANT_BUILDER_PUSH),
					.finalize_func = base::StrID(comptime_func_names::VARIANT_BUILDER_FINALIZE) },
				op.args,
				op.dest
			);
			break;
		case lir::Operation::MetaEq:
			lowerSingleCall(this, ctx, comptime_func_names::TYPES_EQUAL, op.args, op.dest);
			break;
		case lir::Operation::MetaNeq:
			lowerSingleCall(this, ctx, comptime_func_names::TYPES_NOT_EQUAL, op.args, op.dest);
			break;
		default:
			CORE_PANIC("Unknown meta operation: ", base::enumToStr(op.meta_op));
		}
	}

}
