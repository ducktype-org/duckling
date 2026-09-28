/**
 * @file identifiers.hpp
 * @brief Helpers for turning C names into Duckling names.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace c_import {

	/**
	 * @brief Whether `name` cannot be used as a Duckling identifier because it is a keyword, a
	 * named operator or a primitive type or literal name.
	 */
	bool isReservedName(std::string_view name);

	/**
	 * @brief `name` if it can be used as a Duckling identifier, otherwise `name` with `_` appended.
	 * @note Only for names that are not linked (fields, parameters, constants, records).
	 */
	std::string usableName(std::string_view name);

	/// Shell-style match supporting `*` and `?`.
	bool globMatch(std::string_view pattern, std::string_view text);

	/**
	 * @brief Whether `name` passes the filters: it matches one of `include` (or `include` is
	 * empty) and none of `exclude`.
	 */
	bool passesFilters(
		std::string_view                name,
		const std::vector<std::string>& include,
		const std::vector<std::string>& exclude
	);

}
