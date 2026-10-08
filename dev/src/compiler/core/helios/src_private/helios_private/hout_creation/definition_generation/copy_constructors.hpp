// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Get the symbol of the compiler-generated copy constructor for a given type.
	 * This function always returns a SymID, even if the requested type is trivially copyable,
	 * in which case the SymID might be non-usable.
	 */
	SymID generatedCopyConstructorSymForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Get the symbol of the copy constructor of a type, which is the user-defined one when
	 * the type declares it, and the compiler-generated one otherwise.
	 */
	SymID copyConstructorSymForType(query::Context& ctx, tsh::AbstractType type);

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
