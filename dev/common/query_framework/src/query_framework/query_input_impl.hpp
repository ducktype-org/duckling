#pragma once

#include "context.hpp"                   // IWYU pragma: export
#include "detail/dep_graph.hpp"          // IWYU pragma: export
#include "detail/node_making.hpp"        // IWYU pragma: export
#include "detail/query_id_provider.hpp"  // IWYU pragma: export

#define IMPLEMENT_QUERY_SIDE_INPUT(query_type)                                              \
	auto query_type::internal_query(query_type::QKey key, ::query::detail::NodeID from)     \
		-> query_type::QResult {                                                            \
		auto node_id = query::detail::makeNodeID(query_type::id, key);                      \
		query::detail::dep_graph::addDependency(from, node_id);                             \
		query::detail::dep_graph::setEntry(node_id, from);                                  \
		query::detail::dep_graph::setExit(node_id);                                         \
		return query::detail::SideInputMockValue{};                                         \
	}                                                                                       \
	decltype(query_type::id)   query_type::id   = ::query::detail::newQueryID(#query_type); \
	decltype(query_type::name) query_type::name = #query_type;
