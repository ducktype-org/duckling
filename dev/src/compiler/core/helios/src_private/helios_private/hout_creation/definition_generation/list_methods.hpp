#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	SymID pushMethodForType(query::Context& ctx, tsh::AbstractType type);
	SymID popMethodForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Get the compiler-generated HOUT representation of the `push` method for a dynamic
	 * array type. Appends an element to the end of the array, growing the storage if
	 * needed.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryPushMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));

	/**
	 * @brief Get the compiler-generated HOUT representation of the `pop` method for a dynamic array
	 * type. Removes a number of elements from the end of the array.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryPopMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));
}
