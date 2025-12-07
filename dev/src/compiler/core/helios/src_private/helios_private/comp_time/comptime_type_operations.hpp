

#include "typesystem/higher/symbol_type.hpp"

#include "query_framework/context.hpp"
#include "string_id/string_id.hpp"

#include <vm/bytecode/extern_c_function.hpp>

namespace compiler::helios::comptime_ops {
	/**
	 * @brief Builder for constructing variant types.
	 */
	struct VariantTypeBuilder {
		std::vector<tsh::SymbolType<>> subtypes;
	};

	/**
	 * @brief Builder for constructing tuple types.
	 */
	struct TupleTypeBuilder {
		std::vector<tsh::SymbolType<>> subtypes;
	};

	/**
	 * @brief Builder for constructing function types.
	 */
	struct FunctionTypeBuilder {
		std::vector<tsh::SymbolType<>> subtypes;
	};

	/**
	 * @brief Builder for constructing struct types.
	 */
	struct StructTypeBuilder {
		struct Field {
			base::StrID       name;
			tsh::SymbolType<> type;
		};

		std::vector<Field> fields;
	};

	namespace type_ops {
        // comptime_create_box(ctx: opq, b: opq) -> opq
        // comptime_create_ref(ctx: opq, b: opq) -> opq
        // comptime_create_optional(ctx: opq, b: opq) -> opq
        // comptime_create_get_size(ctx: opq, b: opq) -> opq
        // comptime_tuple_builder_new(ctx: opq) -> opq
        // comptime_tuple_builder_push(ctx: opq, b: opq, tp: opq)
        // comptime_tuple_builder_finalize(ctx: opq, b: opq) -> opq 
        // comptime_variant_builder_new(ctx: opq) -> opq
        // comptime_variant_builder_push(ctx: opq, b: opq, tp: opq)
        // comptime_variant_builder_finalize(ctx: opq, b: opq) -> opq 
        // comptime_func_type_builder_new(ctx: opq) -> opq
        // comptime_func_type_builder_push_arg(ctx: opq, b: opq, tp: opq)
        // comptime_func_type_set_ret_type(ctx: opq, b: opq, tp: opq)
        // comptime_func_type_builder_finalize(ctx: opq, b: opq) -> opq
	}


}
