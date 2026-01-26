#pragma once

#include <query_framework/context/context.hpp>                   // IWYU pragma: export
#include <query_framework/internal/context_access.hpp>           // IWYU pragma: export
#include <query_framework/internal/query_data/query_id.hpp>      // IWYU pragma: export
#include <query_framework/internal/query_graph/node_making.hpp>  // IWYU pragma: export
#include <query_framework/utils/query_hash.hpp>                  // IWYU pragma: export

// note PR: we don't care about active graph for inputs, since they don't have dependencies
#define IMPLEMENT_QUERY_SIDE_INPUT(query_type)                                                   \
	auto query_type::internal_query(const query_type::QKey& key, ::query::internal::NodeID from) \
		-> query_type::QResult {                                                                 \
		auto node_id = ::query::internal::makeNodeID<query_type>(key);                           \
		::query::internal::ContextAccess::getState()->getGraphMutable()->addDependency(          \
			from, node_id                                                                        \
		);                                                                                       \
		return ::query::internal::SideInputMockValue{};                                          \
	}                                                                                            \
	static_assert(                                                                               \
		not std::is_reference_v<query_type::QKey>,                                               \
		"Query key type should not be a reference (use custom struct instead)"                   \
	);                                                                                           \
	static_assert(                                                                               \
		::query::HasStablePerfectHash<query_type::QKey>,                                         \
		"queryStablePerfectHash must be implemented for side inputs keys"                        \
	);                                                                                           \
	static_assert(query_type::QueryType::QUERY_DATA.verify(), "Query data is inconsistent.");    \
	decltype(query_type::id) query_type::id                                                      \
		= ::query::internal::registerQuery(query_type::QUERY_DATA);
