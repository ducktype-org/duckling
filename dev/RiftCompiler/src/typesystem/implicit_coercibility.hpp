/**
 * @file implicit_coercibility.hpp
 * @brief query in types are implicitly coercible (not including potential custom implicit
 * constructs)
 */

#pragma once

#include "type_desc.hpp"
#include "type_info.hpp"

// In the future, coercibility could work significantly differently.
// For example, these functions could also return the OperationID of the coercion operation.
// The current coercion implementation has not yet been tested.
// @TODO: Consider the above and add tests

namespace ts {
	void addUserDefinedImplicitCoercion(const TypeInfo& from, const TypeInfo& to);

	[[nodiscard]] bool isImplicitlyCoercible(const TypeInfo& from, const TypeInfo& to);

	[[nodiscard]] bool isImplicitlyCoercible(const TypeDesc<>& from, const TypeDesc<>& to);
}
