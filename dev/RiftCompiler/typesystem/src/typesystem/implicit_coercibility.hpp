/**
 * @file implicit_coercibility.hpp
 * @brief Interface to deducing whether an implicit coercion of two values is allowed.
 *
 * An implicit coercion is when, for example, a boolean is expected, but
 * and integer is given. A desirable (and common) behaviour may be to
 * convert the integer value to true if and only if it is non-zero.
 *
 * Another context in which implicit coercions are desirable is when
 * casting from subclass to superclass.
 *
 * In other words, a coercion is the conversion of a value of one type, to a value of another
 * type, be it with a no-op (cast, if upwards a class hierarchy) or otherwise (conversion).
 *
 * These methods do not determine how to perform a coercion.
 * They only determine whether one should be considered.
 * A coercion may thus be allowed but not implemented; or implemented
 * but not allowed to be used implicitly by the compiler, so the user
 * may define a coercion from class A to class B, but not want it
 * to ever be used implicitly (in C++ that is achieved by annotating a
 * single-argument constructor with the `explicit` keyword).
 *
 * This is typically determined by rules specific for the Kind of the source type and
 * value categories (see: ValueCategory) of the source and target values.
 *
 * The user may also declare that they desire an implicit coercion to be considered.
 */

#pragma once

#include "type_desc.hpp"
#include "type_info.hpp"

#include <map>
#include <set>

// In the future, coercibility could work significantly differently.
// For example, these functions could also return the OperationID of the coercion operation.
// The current coercion implementation has not yet been tested.
// @TODO: Consider the above and add tests

namespace ts {
	std::map<TypeInfo, std::set<TypeInfo>>& getUserDefinedImplicitCoercions();

	/**
	 * @brief Declare that an implicit coercion between two types should be considered.
	 * @param source The source type.
	 * @param target The target type.
	 */
	void addUserDefinedImplicitCoercion(const TypeInfo& source, const TypeInfo& target);

	/**
	 * @brief Check whether an implicit coercion from one type to another is allowed.
	 * @param source The source type.
	 * @param target The target type.
	 * @return Whether an implicit coercion is allowed for the given types.
	 */
	[[nodiscard]]
	bool isImplicitlyCoercible(const TypeInfo& source, const TypeInfo& target);

	/**
	 * @brief Check whether an implicit coercion from one value to another is allowed.
	 * @param source The source value description.
	 * @param target The target value description.
	 * @return Whether an implicit coercion is allowed for the given values.
	 */
	[[nodiscard]]
	bool isImplicitlyCoercible(const TypeDesc<>& source, const TypeDesc<>& target);
}
