#pragma once

#include <query_framework/query_int.hpp>

#include <cstdint>

DECLARE_QUERY(Query1, uint64_t, uint64_t, ({.uses_qresult            = false}))

DECLARE_QUERY(Query2, uint64_t, uint64_t, ({.uses_qresult            = false}))

DECLARE_QUERY(CyclicQuery, uint64_t, uint64_t, ({.uses_qresult            = false}))

/**
 * @brief Simple query extension
 */
u64 squareValue(query::Context&, u64);
