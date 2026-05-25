#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the compiler-generated HOUT representation of a type's destructor.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultDestructor, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);
}
