#pragma once

#include <base/types/ints.hpp>
#include "vm/loader/compiler/bijective_map.hpp"

#include <functional>
#include <vector>

namespace peristent {

	template<typename KeyT, typename ValT>
	class Map {
		struct Node {
			usize key_id = 0;
			usize val_id = 0;
			usize left   = 0;
			usize right  = 0;
		};

		using NodeH = decltype([](Node n) -> usize {
			return std::hash<usize>{}(n.key_id) ^ std::hash<usize>{}(n.key_id)
			     ^ std::hash<usize>{}(n.key_id) ^ std::hash<usize>{}(n.key_id);
		});

		detail::BijectiveMap<usize, Node, std::hash<usize>, NodeH> nodes;

		
	};
}
