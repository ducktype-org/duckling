#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>

#include <vm/loader/compiler/bijective_map.hpp>

#include <ranges>

namespace persistent {
	STRONG_TYPEDEF_INT(VectoStateID, u64);

	/**
	Class implementing a STL vector with time-persistency aka control version. You can modify any of
	the previous instances of the vector, if you know its' `VectoStateID`. `VectoStateID` is
	returned after each operation `pop`, `push`, `change`.
	@note: Two instances may receive the same `VectoStateID` - this happens when via modification
	vector returned to state that it was in previous instance. It allows for == comparison in O(1)
	*/
	template<typename VarT, typename VarH = std::hash<VarT>>
	class Vector {
		using VarNodeID = u64;

		struct NodeChildren {
			VarNodeID left;
			VarNodeID rght;
		};

		using VarNodeH = decltype([](const NodeChildren& h) -> usize {
			return (std::hash<VarNodeID>{}(h.left) << 1) ^ std::hash<VarNodeID>{}(h.rght);
		});

		struct LeafEntry {
			usize idx;
			VarT  var;
			bool  operator==(const LeafEntry&) const = default;
		};

		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ VarH {}(h.var);
		});

		detail::BijectiveMap<NodeChildren, VarNodeID, VarNodeH> var_entries{};
		detail::BijectiveMap<LeafEntry, VarNodeID, LeafEntryH>  leaf_values{};

		struct RootEntry {
			usize height;
			usize size;
		};

		base::HashMap<VarNodeID, RootEntry> root_info{};
		VarNodeID                           next_node_id = 1;

		enum Dir { L, R };

		using Path = std::vector<std::pair<Dir, VarNodeID>>;

		Path getNodePath(VarNodeID root, usize idx) {
			Path ans    = {};
			Dir  dir    = L;
			auto height = root_info[root].height;

			CORE_ASSERT(idx <= root_info[root].size, "The idx was out of bounds!");
			if (height == 0) return {};

			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = var_entries[root];
				auto  orig  = root;
				if (idx & max_bit) {
					dir  = R;
					root = entry.rght;
				} else {
					dir  = L;
					root = entry.left;
				}
				ans.emplace_back(dir, orig);
			}

			return ans;
		}

		VarNodeID nodeFromChildren(VarNodeID left_child, VarNodeID rght_child) {
			auto children       = NodeChildren{ .left = left_child, .rght = rght_child };
			auto [is_new, node] = var_entries.emplaceByLeft(children, next_node_id);
			next_node_id += (is_new ? 0 : 1);

			return node;
		}

		VarNodeID nodeFromIdxVar(usize idx, VarT var) {
			auto leaf           = LeafEntry{ .idx = idx, .var = std::move(var) };
			auto [is_new, node] = leaf_values.emplaceByLeft(leaf, next_node_id);
			next_node_id += (is_new ? 0 : 1);

			return node;
		}

		VarNodeID getChild(VarNodeID node_id, Dir dir) const {
			auto& entry = var_entries.atLeft(node_id);
			return (dir == L) ? entry.left : entry.rght;
		}

	public:
		VectoStateID push(VectoStateID state_id, VarT var) {
			VarNodeID prev_var_root = u64(state_id);
			if (!root_info.contains(prev_var_root))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto prev_size = root_info[prev_var_root].size;
			auto height    = root_info[prev_var_root].height;

			auto var_node = nodeFromIdxVar(prev_size + 1, std::move(var));

			auto prev_path = getNodePath(prev_var_root, prev_size);
			bool merged    = false;

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				VarNodeID left = 0, rght = 0;

				if (merged == (dir == L)) {
					// when !merged and (dir == R) or merged and (dir == L)
					left = var_node;
					rght = 0;  // equiv to entry.right
				} else {
					// when merged and (dir == R) or !merged and (dir == L)
					left   = getChild(node_id, L);
					rght   = var_node;
					merged = true;
				}

				var_node = nodeFromChildren(left, rght);
			}

			if (!merged) {
				var_node = nodeFromChildren(prev_var_root, var_node);
				height++;
			}

			root_info.put(var_node, RootEntry{ .height = height, .size = prev_size + 1 });

			return VectoStateID{ u64(var_node) };
		}

		VectoStateID change(VectoStateID state_id, usize idx, VarT var) {
			VarNodeID prev_var_root = u64(state_id);
			if (!root_info.contains(prev_var_root))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto size   = root_info[prev_var_root].size;
			auto height = root_info[prev_var_root].height;

			if (idx == 0 || idx > size)
				throw std::invalid_argument("received index out of range for given state");

			auto prev_path = getNodePath(prev_var_root, idx);
			auto var_node  = nodeFromIdxVar(idx, std::move(var));

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				VarNodeID left = 0, rght = 0;

				if (dir == L) {
					left = var_node;
					rght = getChild(node_id, R);
				} else {
					left = getChild(node_id, L);
					rght = var_node;
				}

				var_node = nodeFromChildren(left, rght);
			}

			root_info.put(var_node, RootEntry{ .height = height, .size = size });

			return VectoStateID{ u64(var_node) };
		}

		VectoStateID pop(VectoStateID state_id) {
			VarNodeID prev_var_root = u64(state_id);
			if (!root_info.contains(prev_var_root))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto size   = root_info[prev_var_root].size;
			auto height = root_info[prev_var_root].height;

			if (size == 0)
				throw std::invalid_argument("stack at given state is empty - cannot pop from it");

			auto prev_path = getNodePath(prev_var_root, size - 1);
			auto last_path = getNodePath(prev_var_root, size);

			std::vector<std::pair<Dir, VarNodeID>> common = {};

			using namespace std::views;
			for (auto& [f, s]: zip(prev_path, last_path)) {
				if (f != s) break;
				common.emplace_back(f);
			}

			if (common.size() == 0) {
				auto new_root = getChild(prev_var_root, L);
				root_info.put(new_root, RootEntry{ .height = height - 1, .size = size - 1 });

				return new_root;
			}

			auto [last_dir, node_id] = common.back();
			VarNodeID lca            = getChild(node_id, last_dir);

			auto var_node = nodeFromChildren(getChild(lca, L), 0);

			for (auto [dir, node_id]: common | reverse) {
				VarNodeID left = 0, rght = 0;

				if (dir == L) {
					left = var_node;
					rght = getChild(node_id, R);
				} else {
					left = getChild(node_id, L);
					rght = var_node;
				}

				var_node = nodeFromChildren(left, rght);
			}

			root_info.put(var_node, RootEntry{ .height = height, .size = size - 1 });

			return var_node;
		}
	};
}
