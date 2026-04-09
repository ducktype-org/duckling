#pragma once

#include <helios/hout/hout.hpp>
#include <tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the compiler-generated HOUT representation of the implicit constructor for a class.
	 *
	 * The implicit constructor is a function that takes parameters for each field of the class
	 * and returns an instance of the class with those fields initialised accordingly.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryImplicitClassConstructor,
		tsh::ClassAbstractType,
		CRef<query::QResult<HOUTFunction>>,
		({})
	);
}
