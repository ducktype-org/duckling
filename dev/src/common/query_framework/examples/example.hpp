// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <query_framework/query_int.hpp>

#include <cstdint>

struct Key1 {
	u64            v;
	constexpr auto operator<=>(const Key1& oth) const = default;

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		return v;
	}
};


DECLARE_QUERY(Query1, Key1, u64, ({ .uses_qresult = false }))

DECLARE_QUERY(Query2, Key1, u64, ({ .uses_qresult = false }))

DECLARE_QUERY(CyclicQuery, Key1, u64, ({ .uses_qresult = false }))

/**
 * @brief Simple query extension
 */
u64 squareValue(query::Context&, u64);
