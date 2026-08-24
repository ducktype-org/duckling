#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the toString method for a type.
	 */
	SymID toStringSymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Get the symbol of the generated toString method for a type.
	 * Note that this symbol should not be used if the user implemented it's own type.
	 */
	SymID generatedToStringSymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Utility function to get the append String method symbol on String class.
	 *
	 * `String` overloads `append` for both a by-reference and a by-value `String` argument.
	 * @param ctx The query context.
	 * @param arg_by_reference When true, selects the `append(other: ref String)` overload;
	 * when false, selects the `append(other: String)` overload (which consumes its argument).
	 * @return SymID
	 */
	SymID stringAppendMethodSym(query::Context& ctx, bool arg_by_reference);

	/**
	 * @brief Get the compiler-generated HOUT representation of the toString method for a type.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryToStringMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));
}
