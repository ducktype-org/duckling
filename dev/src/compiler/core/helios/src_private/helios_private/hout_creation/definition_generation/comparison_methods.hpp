// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <array>
#include <string_view>

namespace compiler::helios::defgen {
	/**
	 * @brief Describes one of the comparison operators every type has in its default interface.
	 */
	struct ComparisonMethodInfo final {
		Method::Kind           kind;
		tsh::MemberSpecialKind special_kind;
		std::string_view       spelling;
	};

	/**
	 * @brief The comparison operators every type (carrying information) has in its default
	 * interface. Only `==` and `<` are generated from the structure of the type, the others are
	 * defined in terms of them.
	 */
	inline constexpr std::array<ComparisonMethodInfo, 6> COMPARISON_METHODS{ {
		{ .kind         = Method::Kind::Equal,
		  .special_kind = tsh::MemberSpecialKind::Equal,
		  .spelling     = "==" },
		{ .kind         = Method::Kind::NotEqual,
		  .special_kind = tsh::MemberSpecialKind::NotEqual,
		  .spelling     = "!=" },
		{ .kind = Method::Kind::Less, .special_kind = tsh::MemberSpecialKind::Less, .spelling = "<" },
		{ .kind         = Method::Kind::LessEqual,
		  .special_kind = tsh::MemberSpecialKind::LessEqual,
		  .spelling     = "<=" },
		{ .kind         = Method::Kind::Greater,
		  .special_kind = tsh::MemberSpecialKind::Greater,
		  .spelling     = ">" },
		{ .kind         = Method::Kind::GreaterEqual,
		  .special_kind = tsh::MemberSpecialKind::GreaterEqual,
		  .spelling     = ">=" },
	} };

	/**
	 * @brief Get the description of the comparison operator of the given kind.
	 */
	const ComparisonMethodInfo& comparisonMethodInfo(Method::Kind kind);

	/**
	 * @brief Get the symbol of the comparison operator @p kind for a type. This is the one in the
	 * type's interface, so it may be user-defined.
	 */
	SymID comparisonSymForType(query::Context& ctx, tsh::AbstractType type, Method::Kind kind);

	/**
	 * @brief Get the symbol of the generated comparison operator @p kind for a type.
	 * Note that this symbol should not be used if the user implemented their own operator.
	 */
	SymID generatedComparisonSymForType(
		query::Context& ctx, tsh::AbstractType type, Method::Kind kind
	);

	/**
	 * @brief Get the compiler-generated HOUT representation of the `==` operator for a type.
	 * Aggregate types are compared field by field.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryEqualMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));

	/**
	 * @brief Get the compiler-generated HOUT representation of the `<` operator for a type.
	 * Aggregate types are compared lexicographically, in the order of their fields.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryLessMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));

	/**
	 * @brief Get the compiler-generated HOUT representation of the `!=`, `<=`, `>` or `>=`
	 * operator, given its generated symbol. These call the `==` or `<` operator of the type.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryDerivedComparisonMethod, SymID, CRef<query::QResult<HOUTFunction>>, ({}));
}
