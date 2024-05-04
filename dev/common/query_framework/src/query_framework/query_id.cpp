//
// Created by mateusz on 04.05.24.
//
#include "query_id.hpp"

#include <iostream>

namespace query::detail {
	namespace {
		base::HashMap<QueryID::VAL_T, std::string> name_map{};
	}

	const std::string& QueryID::getName() const { return name_map.at(val); }

	void QueryID::setName(const QueryID& query, const std::string_view name) {
		name_map.put(query.val, name);
	}
}
