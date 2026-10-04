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
