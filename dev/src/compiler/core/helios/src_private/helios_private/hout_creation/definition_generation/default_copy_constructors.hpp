#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the default copy constructor for a given type.
	 */
	SymID copySymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Get the compiler-generated HOUT representation of a type's default copy constructor.
	 *
	 * The default copy constructor takes a `const ref T` to the source object and returns a new
	 * copied `T`. Trivially-copyable members are byte-copied by the surrounding assignment
	 * Non-trivially-copyable members are copied by recursively invoking their own copy
	 * constructor.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultCopyConstructor, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);
}
