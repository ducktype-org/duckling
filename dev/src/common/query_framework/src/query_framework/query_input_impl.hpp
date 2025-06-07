#pragma once

#include "context.hpp"                         // IWYU pragma: export
#include "detail/context_access.hpp"           // IWYU pragma: export
#include "detail/query_data/query_id.hpp"      // IWYU pragma: export
#include "detail/query_graph/node_making.hpp"  // IWYU pragma: export

#define IMPLEMENT_QUERY_SIDE_INPUT(query_type)                                                     \
	auto query_type::internal_query(query_type::QKey key, ::query::detail::NodeID from)            \
		-> query_type::QResult {                                                                   \
		auto node_id = query::detail::makeNodeID(query_type::id, key);                             \
		query::detail::ContextAccess::getState()->getGraphMutable()->addDependency(from, node_id); \
		query::detail::ContextAccess::getState()->setEntry(node_id, from);                         \
		query::detail::ContextAccess::getState()->setExit(node_id);                                \
		return query::detail::SideInputMockValue{};                                                \
	}                                                                                              \
	static_assert(                                                                                 \
		not std::is_reference_v<query_type::QKey>,                                                 \
		"Query key type should not be a reference (use custom struct instead)"                     \
	);                                                                                             \
	decltype(query_type::id) query_type::id = ::query::detail::registerQuery(query_type::getData());
