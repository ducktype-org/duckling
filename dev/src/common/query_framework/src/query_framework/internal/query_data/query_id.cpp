// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include "query_data.hpp"

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

namespace query::internal {

	struct QueryIDMaker {
		static constexpr QueryID make(u64 val) { return { QueryID(val) }; }

		static constexpr QueryID next(QueryID id) { return { QueryID(id.val + 1) }; }
	};

	namespace {

		/**
		 * @note It will be used before main, constinit is important.
		 */
		constinit QueryID next = QueryIDMaker::make(0);

		using DataMap = base::VectorMap<QueryID, QueryData>;

		/**
		 * @note Access to data is done this way, to make it safe to use before main.
		 * @note data is not stored directly in QueryID, to keep QueryID light.
		 */
		DataMap& dataMap() {
			static DataMap data_map{};
			return data_map;
		}
	}

	const QueryData& QueryID::getData() const {
		auto opt = dataMap().atMaybe(*this);
		// Only queries loaded from previous graph in incremental compilation might be unregistered
		// (not converted to Dummy queries) so this is a mistake - they should be registered as
		// Dummy before using.
		CORE_ASSERT(
			opt.has_value(), "QueryID not found in dataMap, so it wasn't registered", this->asInt()
		);
		return **opt;
	}

	bool QueryID::registered() const { return dataMap().contains(*this); }

	QueryID registerQuery(QueryData query_data) {
		CORE_ASSERT(
			query_data.kind != QueryKind::Dummy
				|| query_data.tags.used_hashes == UsedHashes::UnstableHash,
			"Dummy queries must use unstable hashes"
		);
		auto ret_id = next;
		next        = QueryIDMaker::next(ret_id);
		dataMap().put(ret_id, query_data);
		return ret_id;
	}
}
