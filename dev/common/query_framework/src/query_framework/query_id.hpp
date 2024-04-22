#pragma once

#include "base/maps.hpp"
#include <base/ints.hpp>

namespace query::detail {
	struct QueryID {
		using VAL_T = u64;
		VAL_T val;

		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}

		[[nodiscard]]
		const std::string& getName() const {
			return name_map.at(val);
		}

		static void setName(const QueryID& query, const std::string& name) {
			name_map.put(query.val, name);
		}

	private:
		// @TODO: does it have one definition? Also we can probably put it in cpp anyway for faster compilation/recompilation
		static inline base::HashMap<VAL_T, std::string> name_map;
	};
}
