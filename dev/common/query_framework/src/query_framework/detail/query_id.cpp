/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include <base/maps.hpp>

#include <string_view>

namespace query::detail {
	namespace {
		using NameMap = base::HashMap<QueryID::VAL_T, std::string>;

		NameMap& nameMap() {
			static base::HashMap<QueryID::VAL_T, std::string> name_map{};
			return name_map;
		}
	}

	std::string_view QueryID::getName() const { return nameMap().at(val); }

	void QueryID::setName(QueryID query, std::string_view name) {
		CORE_ASSERT(
			not nameMap().contains(query.val), "Query name already set for query id: ", query.val
		);
		nameMap().put(query.val, name);
	}
}
