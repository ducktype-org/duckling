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

#include <algorithm>
#include <array>
#include <ranges>
#include <string_view>
#include <utility>

namespace compiler::helios::defgen {
	enum class ComparisonKind : unsigned {
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
		Method::Kind           defgen_kind;
		tsh::MemberSpecialKind special_kind;
		std::string_view       spelling;
	};

	/**
	 * @brief The comparison operators that every type has in its default interface, in the order
	 * of ComparisonKind.
	 */
	inline constexpr std::array<ComparisonMethodInfo, 6> COMPARISON_METHODS{ {
		{ .kind         = ComparisonKind::Equal,
		  .defgen_kind  = Method::Kind::Equal,
		  .special_kind = tsh::MemberSpecialKind::Equal,
		  .spelling     = "==" },
		{ .kind         = ComparisonKind::NotEqual,
		  .defgen_kind  = Method::Kind::NotEqual,
		  .special_kind = tsh::MemberSpecialKind::NotEqual,
		  .spelling     = "!=" },
		{ .kind         = ComparisonKind::Lesser,
		  .defgen_kind  = Method::Kind::Lesser,
		  .special_kind = tsh::MemberSpecialKind::Lesser,
		  .spelling     = "<" },
		{ .kind         = ComparisonKind::LesserEqual,
		  .defgen_kind  = Method::Kind::LesserEqual,
		  .special_kind = tsh::MemberSpecialKind::LesserEqual,
		  .spelling     = "<=" },
		{ .kind         = ComparisonKind::Greater,
		  .defgen_kind  = Method::Kind::Greater,
		  .special_kind = tsh::MemberSpecialKind::Greater,
		  .spelling     = ">" },
		{ .kind         = ComparisonKind::GreaterEqual,
		  .defgen_kind  = Method::Kind::GreaterEqual,
		  .special_kind = tsh::MemberSpecialKind::GreaterEqual,
		  .spelling     = ">=" },
	} };

	static_assert(
		std::ranges::all_of(
			std::views::iota(usize{ 0 }, COMPARISON_METHODS.size()),
			[](const usize i) { return std::to_underlying(COMPARISON_METHODS.at(i).kind) == i; }
		),
		"COMPARISON_METHODS must be ordered by ComparisonKind."
	);

	/**
	 * @brief Get the description of the comparison operator @p kind.
	 */
	constexpr const ComparisonMethodInfo& comparisonMethodInfo(const ComparisonKind kind) {
		return COMPARISON_METHODS.at(std::to_underlying(kind));
	}

	/**
	 * @brief Get the description of the generated comparison operator @p defgen_kind.
	 * @note Panics if @p defgen_kind is not a comparison operator.
	 */
	const ComparisonMethodInfo& comparisonMethodInfo(Method::Kind defgen_kind);

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
