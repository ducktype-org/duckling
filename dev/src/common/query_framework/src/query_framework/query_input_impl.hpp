#pragma once

#include "context.hpp"                           // IWYU pragma: export
#include "internal/context_access.hpp"           // IWYU pragma: export
#include "internal/query_data/query_id.hpp"      // IWYU pragma: export
#include "internal/query_graph/node_making.hpp"  // IWYU pragma: export
#include "query_hash.hpp"                        // IWYU pragma: export

// PR note: this does not speed up, which is unexpected.

#include <set>
#include <unordered_set>

namespace query::internal {
	struct SideInputID {
		::query::internal::NodeID from;
		::query::QueryStableHash  key_hash;

		bool operator<(const SideInputID& other) const {
			if (from != other.from) return from < other.from;
			return key_hash < other.key_hash;
		}

		bool operator==(const SideInputID& other) const {
			return from == other.from && key_hash == other.key_hash;
		}
	};
}

template<>
struct std::hash<query::internal::SideInputID> {
	size_t operator()(const query::internal::SideInputID& id) const noexcept {
		size_t h1 = std::hash<query::internal::NodeID>()(id.from);
		size_t h2 = std::hash<query::QueryStableHash>()(id.key_hash);
		return h1 ^ (h2 << 1);
	}
};

// auto key_hash = query::perfectHashKey(key);                      \
		// 	if (ImplementationOf_##query_type::registered_calls.contains({from, key_hash})) {    \
		// 		return query::internal::SideInputMockValue{}; \
		// 	}                                                                    \
		// 	ImplementationOf_##query_type::registered_calls.insert({from, key_hash});   \


#define IMPLEMENT_QUERY_SIDE_INPUT(query_type)                                                   \
                                                                                                 \
	namespace {                                                                                  \
		namespace ImplementationOf_##query_type {                                                \
			std::unordered_set<query::internal::SideInputID> registered_calls;                   \
		}                                                                                        \
	}                                                                                            \
                                                                                                 \
	auto query_type::internal_query(const query_type::QKey& key, ::query::internal::NodeID from) \
		-> query_type::QResult {                                                                 \
		auto node_id = query::internal::makeNodeID(query_type::id, key);                         \
		query::internal::ContextAccess::getState()->getGraphMutable()->addDependency(            \
			from, node_id                                                                        \
		);                                                                                       \
		query::internal::ContextAccess::getState()->setEntry(node_id, from);                     \
		query::internal::ContextAccess::getState()->setExit(node_id);                            \
		return query::internal::SideInputMockValue{};                                            \
	}                                                                                            \
	static_assert(                                                                               \
		not std::is_reference_v<query_type::QKey>,                                               \
		"Query key type should not be a reference (use custom struct instead)"                   \
	);                                                                                           \
	static_assert(                                                                               \
		::query::HasStablePerfectHash<query_type::QKey>,                                         \
		"queryStablePerfectHash must be implemented for side inputs keys"                        \
	);                                                                                           \
	decltype(query_type::id) query_type::id                                                      \
		= ::query::internal::registerQuery(query_type::getData());
