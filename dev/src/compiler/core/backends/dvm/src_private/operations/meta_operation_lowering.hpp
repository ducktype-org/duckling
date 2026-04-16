#pragma once
#include "dvm_operation.hpp"
#include "dvm_value.hpp"

namespace compiler::backend_vm::internal {
	class FunctionLoweringContext;

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

	class MetaOperationLowerer {
	public:
		explicit MetaOperationLowerer(FunctionLoweringContext& func_ctx): func_ctx(func_ctx) {}

		/**
		 * @brief Lowers a meta operation instruction to DVM bytecode.
		 */
		void lower(const MetaOperation& meta_operation);

	private:
		FunctionLoweringContext& func_ctx;

		void lowerCreateBox(const DVMValue& type_arg, const base::Optional<DVMPlace>& output);
		void lowerCreateRef(const DVMValue& type_arg, const base::Optional<DVMPlace>& output);
		void lowerCreateConst(const DVMValue& type_arg, const base::Optional<DVMPlace>& output);
		void lowerCreateTuple(
			const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
		);
		void lowerCreateVariant(
			const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
		);
		void lowerTypesEqual(
			const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
		);
		void lowerTypesNotEqual(
			const std::deque<DVMValue>& type_args, const base::Optional<DVMPlace>& output
		);

		struct BuilderSequence {
			base::StrID new_func;
			base::StrID push_func;
			base::StrID finalize_func;
		};

		void lowerBuilderPattern(
			const BuilderSequence&          builder,
			const std::deque<DVMValue>&     type_args,
			const base::Optional<DVMPlace>& output
		);

		/**
		 * @brief Initializes a local variable of type `opaque_ptr` and stores the global query
		 * context value in it. Returns the created DVMValue.
		 */
		DVMValue getQueryContext();
	};
}
