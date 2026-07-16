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
	 * @brief Get the "concatenate" Strings function symbol.
	 */
	SymID concatSym(query::Context& ctx);

	/**
	 * @brief Get the compiler-generated HOUT representation of the toString method for a type.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryToStringMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));
}
