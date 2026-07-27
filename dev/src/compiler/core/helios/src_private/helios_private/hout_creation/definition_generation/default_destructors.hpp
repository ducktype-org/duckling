#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the compiler-generated destructor for a given abstract type.
	 * This function always returns SymID, even if the requested type is trivially destructible,
	 * in which case the SymID might be non-usable.
	 */
	SymID destructSymForType(query::Context& ctx, tsh::AbstractType type);


	/**
	 * @brief Get the symbol of the proper destructor for a given symbol type (not the optional user one).
	 * Returns empty value when the type doesn't have a destructor (trivial destructor).
	 */
	base::Optional<SymID> destructSymForSymbolType(query::Context& ctx, tsh::SymbolType<> type);

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
	 * The destructor takes a `ref T self` parameter and returns unit. Destroys each
	 * non-trivially-destructible member by invoking it's destructor.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultDestructor, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);

	/**
	 * @brief Build the compiler-generated HOUT representation of the `box T` destructor.
	 *
	 * Takes a `self: box T` parameter and returns unit. Destroys the pointee (via its own
	 * destructor) and then frees the box storage with the `box_free` builtin.
	 *
	 * @param pointee_type The pointee type `T`.
	 */
	HOUTFunction buildBoxDestructor(query::Context& ctx, tsh::AbstractType pointee_type);
}
