// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lookup_in_type_interface.hpp
 * @brief The lookups performed in the interface of a type, that is what `obj.x` and `T.x` do.
 *
 * These are used through @ref HInterface, by `HInterface::ofTypeInstance` and
 * `HInterface::ofTypeMeta` respectively, and should not be queried directly.
 */
#pragma once

#include "lookup_result.hpp"

#include <helios/scope_id.hpp>
#include <helios/tsh/abstract_type.hpp>

#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

namespace compiler::helios {

	/**
	 * @brief Whether the lookup is performed on a value of a type, as in `obj.x`, or on the
	 * type itself, as in `T.x`.
	 */
	enum class TypeAccessMode { Instance, Meta };

	struct KeyOf_LookupInType final {
		tsh::AbstractType type;
		base::StrID       name;

		/**
		 * @brief What the name is looked up on, the value of the type or the type itself.
		 */
		TypeAccessMode mode;

		/**
		 * @brief The scope the lookup is written in, deciding what is visible to it.
		 *
		 * It is empty only for a lookup that does not come from the source code, for example
		 * generated one.
		 */
		base::Optional<ScopeID> accessing_scope;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return { type.queryUnstablePerfectHash(),
				     name.getInnerID().asInt(),
				     accessing_scope.map(&ScopeID::queryUnstablePerfectHash).copyValueOr(0),
				     static_cast<u64>(mode) };
		}
	};

	/**
	 * @brief Look a name up in the interface of a type.
	 *
	 * `TypeAccessMode::Instance` finds the fields and the methods of the type, static or not, as
	 * a value of the type has all of them. `TypeAccessMode::Meta` finds only what does not need
	 * an instance, that is the static fields, the static methods and the other elements declared
	 * in the body of the type.
	 *
	 * The elements hidden by their visibility are reported in `LookupResult::inaccessible`
	 * instead of being dropped, so that the error can say that the name exists but cannot be used.
	 *
	 * In #1392 this may need to be placed in a more appropriate location.
	 *
	 * See https://docs.duckling.pl/duckling/lookup/name_lookup.html
	 * for more info on type lookups.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryLookupInType, KeyOf_LookupInType, CRef<query::QResult<LookupResult>>, ({}))
}
