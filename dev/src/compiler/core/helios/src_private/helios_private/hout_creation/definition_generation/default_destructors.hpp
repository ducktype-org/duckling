#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include "helios/tsh/symbol_type.hpp"

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the compiler-generated destructor for a given abstract type.
	 */
	SymID destructSymForType(query::Context& ctx, tsh::AbstractType type);


	/**
	 * @brief Get the symbol of the compiler-generated destructor for a given symbol type.
	 * Returns empty value when the type is a reference type.
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
}
