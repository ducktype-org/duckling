#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the compiler-generated HOUT representation of the tuple constructor.
	 *
	 * The "packing" constructor is a function that takes parameters for each component of the tuple
	 * and returns an instance of the tuple with those components initialised accordingly.
	 *
	 * @note This is basically QueryImplicitClassConstructor copy-pasted, but classes' ctors might
	 * get more complex, while tuples' will probably stay as is.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryTuplePackConstructor, tsh::TupleAbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);
}
