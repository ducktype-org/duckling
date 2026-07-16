#include "../dvm_value.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <function_lowering_context.hpp>

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <deque>

/**
 * @brief Names of comptime type operation functions in DVM.
 */
namespace comptime_func_names {
	constexpr auto GLOBAL_QUERY_CONTEXT     = "comptime_query_ctx";
	constexpr auto CREATE_BOX               = "comptime_create_box";
	constexpr auto CREATE_REF               = "comptime_create_ref";
	constexpr auto CREATE_CONST             = "comptime_create_const";
	constexpr auto CREATE_PTR               = "comptime_create_ptr";
	constexpr auto CREATE_MANY_PTR          = "comptime_create_many_ptr";
	constexpr auto CREATE_CPTR              = "comptime_create_cptr";
	constexpr auto CREATE_SLICE             = "comptime_create_slice";
	constexpr auto SIZE_OF                  = "comptime_size_of";
	constexpr auto ALIGN_OF                 = "comptime_align_of";
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
		return { DVMPlace(
			base::StrID(comptime_func_names::GLOBAL_QUERY_CONTEXT),
			vm::code::OpaqueType(base::StrID("opaque_ptr"), Bytes{ 8 }),
			DVMPlace::AccessKind::Direct
		) };
	}
}

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const MetaOperation& op) {
		// These two helpers are defined here since they use private FunctionLoweringContext members
		// and we don't want to friend.
		auto lower_single_call = [&](base::StrID                     func_name,
		                             const std::deque<DVMValue>&     args,
		                             const base::Optional<DVMPlace>& output) {
			CallOperation call_op{
				.call_info = FunctionCallInfo::fromExternCFunction(func_name, ctx->program_context),
				.args      = args,
				.dest      = output,

			};
			lower(call_op);
		};

		// Meta constructors that call the type system (ptr / cptr / manyptr / slice) need the query
		// context passed as the first argument, followed by the single type argument.
		auto lower_ctx_call = [&](base::StrID func_name) {
			CORE_ASSERT(op.args.size() == 1, "Meta type-constructor expects 1 argument");
			std::deque<DVMValue> args = { getQueryContext(), op.args.front() };
			lower_single_call(func_name, args, op.dest);
		};

		auto lower_builder_pattern = [&](const BuilderSequence& builder_sequence) {
			auto builder = ctx->pushTempLocal(
				vm::code::OpaqueType(base::StrID("opaque_ptr"), Bytes{ 8 }), "meta_builder"
			);
			DVMPlace builder_place = builder;

			lower_single_call(builder_sequence.new_func, std::deque<DVMValue>(), builder_place);

			DVMValue builder_value = { builder };
			for (const auto& type_arg: op.args)
				lower_single_call(builder_sequence.push_func, { builder_value, type_arg }, {});

			auto query_ctx_val = getQueryContext();
			lower_single_call(
				builder_sequence.finalize_func, { query_ctx_val, builder_value }, op.dest
			);
		};

		switch (op.meta_kind) {
		case lir::MetaKind::CreateBox:
			CORE_ASSERT(op.args.size() == 1, "MetaKind::CreateBox expects 1 argument");
			lower_single_call(base::StrID(comptime_func_names::CREATE_BOX), op.args, op.dest);
			break;
		case lir::MetaKind::CreateRef:
			CORE_ASSERT(op.args.size() == 1, "MetaKind::CreateRef expects 1 argument");
			lower_single_call(base::StrID(comptime_func_names::CREATE_REF), op.args, op.dest);
			break;
		case lir::MetaKind::CreateConst:
			CORE_ASSERT(op.args.size() == 1, "MetaKind::CreateConst expects 1 argument");
			lower_single_call(base::StrID(comptime_func_names::CREATE_CONST), op.args, op.dest);
			break;
		case lir::MetaKind::CreatePtr:
			lower_ctx_call(base::StrID(comptime_func_names::CREATE_PTR));
			break;
		case lir::MetaKind::CreateManyPtr:
			lower_ctx_call(base::StrID(comptime_func_names::CREATE_MANY_PTR));
			break;
		case lir::MetaKind::CreateCPtr:
			lower_ctx_call(base::StrID(comptime_func_names::CREATE_CPTR));
			break;
		case lir::MetaKind::CreateSlice:
			lower_ctx_call(base::StrID(comptime_func_names::CREATE_SLICE));
			break;
		case lir::MetaKind::CreateTuple: {
			auto builder = BuilderSequence{
				.new_func      = base::StrID(comptime_func_names::TUPLE_BUILDER_NEW),
				.push_func     = base::StrID(comptime_func_names::TUPLE_BUILDER_PUSH),
				.finalize_func = base::StrID(comptime_func_names::TUPLE_BUILDER_FINALIZE)
			};
			lower_builder_pattern(builder);
			break;
		}
		case lir::MetaKind::CreateVariant: {
			auto builder = BuilderSequence{
				.new_func      = base::StrID(comptime_func_names::VARIANT_BUILDER_NEW),
				.push_func     = base::StrID(comptime_func_names::VARIANT_BUILDER_PUSH),
				.finalize_func = base::StrID(comptime_func_names::VARIANT_BUILDER_FINALIZE)
			};
			lower_builder_pattern(builder);
			break;
		}
		case lir::MetaKind::Eq:
			lower_single_call(base::StrID(comptime_func_names::TYPES_EQUAL), op.args, op.dest);
			break;
		case lir::MetaKind::Neq:
			lower_single_call(base::StrID(comptime_func_names::TYPES_NOT_EQUAL), op.args, op.dest);
			break;
		case lir::MetaKind::SizeOf:
			lower_ctx_call(base::StrID(comptime_func_names::SIZE_OF));
			break;
		case lir::MetaKind::AlignOf:
			lower_ctx_call(base::StrID(comptime_func_names::ALIGN_OF));
			break;
		}
	}

}
