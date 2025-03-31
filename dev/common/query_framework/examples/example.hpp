#pragma once

#include <cstdint>

#include <query_framework/query_int.hpp>

DECLARE_QUERY(Query1, uint64_t, uint64_t)

DECLARE_QUERY(Query2, uint64_t, uint64_t)

DECLARE_QUERY(CyclicQuery, uint64_t, uint64_t)

/**
 * @brief Simple query extension
 */
u64 SquareValue(query::Context&, u64);
