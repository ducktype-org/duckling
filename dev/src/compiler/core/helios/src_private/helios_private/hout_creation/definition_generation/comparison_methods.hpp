// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>

#include <base/collections/optional.hpp>

#include <lexer/token_common.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	/**
	 * @brief Maps a comparison operator spelling (`==`, `!=`, `<`, `<=`, `>`, `>=`) to the kind of
	 * the generated method implementing it. Returns nothing for any other operator.
	 */
	base::Optional<Method::Kind> comparisonKindForOperator(lexer::Operator op);

	/**
	 * @brief Get the symbol of the generated comparison operator @p kind for an aggregate type.
	 * The operator takes both operands as `const ref` and returns `bool`.
	 */
	SymID generatedComparisonSymForType(
		query::Context& ctx, tsh::AbstractType type, Method::Kind kind
	);

	/**
	 * @brief Whether the compiler can generate the comparison operator @p kind for @p type.
	 *
	 * Only tuples and classes carrying information get generated comparisons. `==` needs `==` on
	 * every field and `<` needs `<` on every field. The other operators are derived from those
	 * two: `!=` is `not (a == b)`, `>` is `b < a`, `<=` is `not (b < a)` and `>=` is
	 * `not (a < b)`.
	 */
	bool canGenerateComparison(query::Context& ctx, tsh::AbstractType type, Method::Kind kind);

	/**
	 * @brief Get the compiler-generated HOUT representation of a comparison operator of an
	 * aggregate type. The key is the generated comparison symbol. `==` and `<` compare the fields
	 * lexicographically, in declaration order.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryComparisonMethod, SymID, CRef<query::QResult<HOUTFunction>>, ({}));
}
