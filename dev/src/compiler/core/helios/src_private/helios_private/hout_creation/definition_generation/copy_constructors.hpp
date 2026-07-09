#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the default copy constructor for a given type.
	 */
	SymID copyConstructorSymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Whether the given symbol is a user-defined copy constructor.
	 */
	bool isUserDefinedCopyConstructor(query::Context& ctx, SymID sym);

	/**
	 * @brief Finds the user-defined copy constructor of a class, if it declares one.
	 * @param class_sym The symbol of the class.
	 * @return The copy constructor symbol, or an empty optional if the class doesn't declare one.
	 */
	base::Optional<SymID> userCopyConstructorOf(query::Context& ctx, SymID class_sym);

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
