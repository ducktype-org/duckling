// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file query_int.hpp
 * @brief Implementation of automatic generation of query inputs.
 */

#pragma once

#include <query_framework/query_int.hpp>  // IWYU pragma: export

namespace query::internal {
	/**
	 * @brief Dummy value used for side-inputs return values.
	 */
	struct SideInputMockValue {};
}

/**
 * @brief Macro used do delcare queries side-inputs.
 *
 * Query side-inputs are special queries that have non-empty keys,
 * but act as a query-input. Their "input" is essentially a query-key,
 * rather then query output.
 *
 * When the side-input is called, new node-id (storing key-hash) is created
 * and added to the dependency graph.
 * It then allows other tools to detect it and read its NodeID to see
 * what input was used (based on its hash).
 *
 * It is in a way dual to key-dependencies.
 *
 * It allows to easily set query-input from non-query components like PST
 * without an additional query-layer in between. Data getters can simple use query-side-input
 * accordingly.
 *
 * @note PST currently use it to mark a query input when data from given PST element is used.
 *
 * @note Query side-inputs have to be called by hand, when given data is read.
 * Special care should be taken to always do it, to prevent non registered input being used.
 */
#define DECLARE_QUERY_SIDE_INPUT(query_type, key)                            \
	DECLARE_QUERY_AUX(                                                       \
		query_type,                                                          \
		key,                                                                 \
		::query::internal::SideInputMockValue,                               \
		::query::internal::QueryData(                                        \
			::query::internal::QueryKind::SideInput,                         \
			#query_type,                                                     \
			::query::internal::QueryTags{                                    \
				.used_hashes             = query::UsedHashes::StableHash,    \
				.can_be_loaded_from_disk = false,                            \
				.preserve_in_graph       = true,                             \
				.uses_qresult            = false,                            \
			},                                                               \
			{                                                                \
				.erase_function      = query_type::internal_erase,           \
				.disk_erase_function = ::query::internal::panicUnwiredErase, \
			}                                                                \
		)                                                                    \
	)
