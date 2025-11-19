/**
 * @file node_making.hpp
 * @brief Helper functions for generating query `NodeID`.
 */
#pragma once

#include "../../query_hash.hpp"
#include "node_id.hpp"

#include <base/types/bit256.hpp>

#include <query_framework/internal/query_data/query_id.hpp>

namespace query::internal {

	/**
	 * @brief Helper function for construing NodeID from the query key.
	 *
	 * @TODO PR: update usage of this function.
	 */
	template<typename QueryInteface>
	internal::NodeID makeNodeID(const typename QueryInteface::QKey& key) {
		static_assert(
			QueryInteface::QUERY_INTERFACE_TAG, "makeNodeID can be used only with query interfaces"
		);

		return NodeID(
			QueryInteface::getID(),
			{ .val = perfectHashKey<QueryInteface::QUERY_DATA.tags.usesStableHashing()>(key) }
		);
	}
}
