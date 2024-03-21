#pragma once

#include <query_framework/query_int.hpp>
#include <cstdint>

struct Query1: query::QueryInterface<
	Query1,    // self
	uint64_t,  // key
	uint64_t   // val
> { QUERY_INTERFACE_BOILERPLATE };

// Is this better or worse?
DECLARE_QUERY (Query2, uint64_t, uint64_t)


DECLARE_QUERY (CyclicQuery, uint64_t, uint64_t)


