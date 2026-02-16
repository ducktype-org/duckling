/**
 * @file node_making.hpp
 * @brief Helper functions for generating query `NodeID`.
 */
#pragma once

#include "../../utils/query_hash.hpp"
#include "node_id.hpp"

#include <base/types/bit256.hpp>

#include <query_framework/internal/query_data/query_id.hpp>

namespace query::internal {

	/**
	 * @brief Helper function for construing NodeID from the query key.
	 */
	template<typename QueryInteface>
	internal::NodeID makeNodeID(const typename QueryInteface::QKey& key) {
		static_assert(
			QueryInteface::QUERY_INTERFACE_TAG, "makeNodeID can be used only with query interfaces"
		);

		return NodeID(
			QueryInteface::getID(),
			KeyHash{ .val = perfectHashKey<QueryInteface::QUERY_DATA.usesStableHashing()>(key) }
		);
	}
}
