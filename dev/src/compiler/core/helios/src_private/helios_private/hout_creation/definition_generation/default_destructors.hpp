#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the compiler-generated destructor for a given type.
	 *
	 * The returned symbol always refers to the generated `__destruct` method. For a class that
	 * declares its own destructor, the generated `__destruct` runs the user code first and then
	 * destroys the members.
	 */
	SymID destructSymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Whether the given symbol is a user-defined destructor.
	 */
	bool isUserDefinedDestructor(query::Context& ctx, SymID sym);

	/**
	 * @brief Finds the user-defined destructor of a class, if it declares one.
	 * @param class_sym The symbol of the class.
	 * @return The destructor symbol, or an empty optional if the class doesn't declare one.
	 */
	base::Optional<SymID> userDestructorOf(query::Context& ctx, SymID class_sym);

	/**
	 * @brief Get the compiler-generated HOUT representation of a type's destructor.
	 *
	 * The destructor takes a single `ref T self` and returns unit. Destroys each
	 * non-trivially-destructible member by recursively invoking it's destructor.
	 * `box T` members destroy their pointee and frees the heap memory. For a
	 * class that declares a user-defined destructor, the user code runs before the members are
	 * destroyed.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultDestructor, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);
}
