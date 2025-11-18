#pragma once

#include <helios/hout/hout.hpp>
#include <typesystem/higher/types.hpp>

#include <query_framework/query_int.hpp>

namespace compiler::helios::houtgen {
	/**
	 * @brief Get the compiler-generated HOUT representation of the implicit constructor for a class.
	 *
	 * The implicit constructor is a function that takes parameters for each field of the class
	 * and returns an instance of the class with those fields initialised accordingly.
	 */
	DECLARE_QUERY(QueryImplicitClassConstructor, tsh::ClassAbstractType, CRef<HOUTFunction>, {});
}
