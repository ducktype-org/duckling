#pragma once

#include "base/maps.hpp"
#include <base/ints.hpp>
#include <deque>
#include <type_traits>

namespace base {
	template<class T, class TID>
	requires std::is_constructible_v<usize, TID> && std::is_constructible_v<TID, usize>
	class TypeIdNameMap {
	public:
		TypeIdNameMap(const TypeIdNameMap&)            = default;
		TypeIdNameMap(TypeIdNameMap&&)                 = default;
		TypeIdNameMap& operator=(const TypeIdNameMap&) = default;
		TypeIdNameMap& operator=(TypeIdNameMap&&)      = default;

		TID add(T&& new_value) {
			auto id = TID(values.size());
			values.push_back(std::move(new_value));
			types_ids.push_back(id);

			names_to_type.put(getType(id)->getName(), id);

			return &types.back();
		}


	private:
		/**
		 * @note We are using std::deque here, because references to its data are always valid. (Do
		 * not become dangling). We also assume that we **never pop** from this queue.
		 */
		std::deque<T>                   values{};
		std::vector<TID>                value_ids{};
		base::HashMap<base::StrID, TID> name_to_ids{};
	};
}
