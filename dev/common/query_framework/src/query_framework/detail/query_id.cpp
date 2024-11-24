/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include <base/maps.hpp>

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
		nameMap().put(query.val, name);
	}

	namespace {
		QueryID           next                = { 1 };
		constexpr QueryID outside_world_query = { 0 };
	}

	QueryID newQueryID(const std::string_view pretty_name) {
		const QueryID ret = next;
		QueryID::setName(ret, pretty_name);

		next.val++;

		return ret;
	}

	QueryID outsideWorldQueryID() { return outside_world_query; }
}
