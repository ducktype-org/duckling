// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <array>
#include <string_view>

namespace compiler::helios::defgen {
	enum class ComparisonKind {
		Equal,
		NotEqual,
		Lesser,
		LesserEqual,
		Greater,
		GreaterEqual,
	};

	/**
	 * @brief Describes one of the comparison operators every type has in its default interface.
	 */
	struct ComparisonMethodInfo final {
		ComparisonKind         kind;
		tsh::MemberSpecialKind special_kind;
		std::string_view       spelling;
	};

	/**
	 * @brief The comparison operators that every type has in its default interface.
	 */
	inline constexpr std::array<ComparisonMethodInfo, 6> COMPARISON_METHODS{ {
		{ .kind         = ComparisonKind::Equal,
		  .special_kind = tsh::MemberSpecialKind::Equal,
		  .spelling     = "==" },
		{ .kind         = ComparisonKind::NotEqual,
		  .special_kind = tsh::MemberSpecialKind::NotEqual,
		  .spelling     = "!=" },
		{ .kind         = ComparisonKind::Lesser,
		  .special_kind = tsh::MemberSpecialKind::Lesser,
		  .spelling     = "<" },
		{ .kind         = ComparisonKind::LesserEqual,
		  .special_kind = tsh::MemberSpecialKind::LesserEqual,
		  .spelling     = "<=" },
		{ .kind         = ComparisonKind::Greater,
		  .special_kind = tsh::MemberSpecialKind::Greater,
		  .spelling     = ">" },
		{ .kind         = ComparisonKind::GreaterEqual,
		  .special_kind = tsh::MemberSpecialKind::GreaterEqual,
		  .spelling     = ">=" },
	} };

	/**
	 * @brief Get the symbol of the comparison operator @p kind for a type. This is the one in the
	 * type's interface, so it may be user-defined.
	 */
	SymID comparisonSymForType(query::Context& ctx, tsh::AbstractType type, ComparisonKind kind);

	/**
	 * @brief Get the symbol of the generated comparison operator @p kind for a type.
	 * Note that this symbol should not be used if the user implemented their own operator.
	 */
	SymID generatedComparisonSymForType(
		query::Context& ctx, tsh::AbstractType type, ComparisonKind kind
	);

	/**
	 * @brief Get the compiler-generated HOUT representation of the
	 * `==`, `!=`, `<`, `<=`, `>` or `>=` operator, given its generated symbol.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryGeneratedComparisonMethod, SymID, CRef<query::QResult<HOUTFunction>>, ({}));
}
