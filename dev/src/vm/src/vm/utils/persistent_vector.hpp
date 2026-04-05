#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>

#include <vm/utils/bijective_map.hpp>

#include <ranges>
#include <utility>
#include <vector>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(VectorStateID, u64);

	/**
	 * @brief Class implementing a STL vector with time-persistency aka control version. You can
	 * modify any of the previous instances of the vector, by using `VectorStateID` which is unique
	 * to the state of the vector.
	 *
	 * @note Implementation based of persistent segment tree.
	 * @note Held values are constructed only once, and nodes hold their id's. This is to allow for
	 * quick construction of leaf elements and to avoid any assumptions about the hash function of
	 * values.
	 *
	 * @tparam VarT type held in the vector
	 * @tparam VarH hash object for VarT
	 */
	template<typename VarT, typename VarH = std::hash<VarT>>
	class Vector {
		using NodeID = u64;

		struct NodeEntry {
			NodeID left;
			NodeID rght;

			bool operator==(const NodeEntry&) const = default;
		};

		using NodeEntryH = decltype([](const NodeEntry& h) -> usize {
			return (std::hash<NodeID>{}(h.left) << 1) ^ std::hash<NodeID>{}(h.rght);
		});

		struct LeafEntry {
			usize idx;
			usize var_id;

			bool operator==(const LeafEntry&) const = default;
		};

		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.var_id);
		});

		struct RootEntry {
			usize height;
			usize size;
		};

		enum class Dir { Left, Right };

		using Path = std::vector<std::pair<Dir, NodeID>>;

		detail::BijectiveMap<NodeEntry, NodeID, NodeEntryH> node_entries{};
		detail::BijectiveMap<LeafEntry, NodeID, LeafEntryH> leaf_entries{};
		detail::BijectiveMap<VarT, usize, VarH>             held_values{};

		base::HashMap<NodeID, RootEntry> root_info{};
		NodeID                           next_node_id = 1;

		Path getNodePath(NodeID root, usize idx) {
			Path ans    = {};
			Dir  dir    = Dir::Left;
			auto height = root_info[root].height;

			CORE_ASSERT(idx <= root_info[root].size, "The idx was out of bounds!");
			if (height == 0) return {};

			auto node = root;
			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				auto  orig  = node;
				if (idx & max_bit) {
					dir  = Dir::Right;
					node = entry.rght;
				} else {
					dir  = Dir::Left;
					node = entry.left;
				}
				ans.emplace_back(dir, orig);
			}

			return ans;
		}

		NodeID nodeFromChildren(NodeID left, NodeID rght) {
			auto children       = NodeEntry{ .left = left, .rght = rght };
			auto [is_new, node] = node_entries.emplaceByLeft(children, next_node_id);
			next_node_id += (is_new ? 1 : 0);

			return node;
		}

		NodeID nodeFromIdxVar(usize idx, const VarT& var) {
			auto [_, var_id]    = held_values.emplaceByLeft(var, held_values.size());
			auto leaf           = LeafEntry{ .idx = idx, .var_id = var_id };
			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf, next_node_id);
			next_node_id += (is_new ? 1 : 0);

			return node;
		}

		NodeID getChild(NodeID node_id, Dir dir) const {
			auto& entry = node_entries.atRight(node_id);
			return (dir == Dir::Left) ? entry.left : entry.rght;
		}

		auto getRootInfo(VectorStateID state_id) const {
			auto node_id = NodeID{ u64(state_id) };
			if (!root_info.contains(node_id))
				throw std::invalid_argument("PersistentVector got invalid state");

			auto& entry = root_info[node_id];

			return std::make_tuple(NodeID{ u64(state_id) }, entry.size, entry.height);
		}

	public:
		const VarT& access(VectorStateID state_id, usize idx) const {
			auto [node, size, height] = getRootInfo(state_id);

			if (idx == 0 || idx > size)
				throw std::invalid_argument("received index out of range for given state");
			CORE_ASSERT(height > 0, "Root of non-empty vector should always have non-zero height");

			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				node        = (idx & max_bit) ? entry.rght : entry.left;
			}

			auto var_id = leaf_entries.atRight(node).var_id;

			return held_values.atRight(var_id);
		}

		[[nodiscard]]
		usize size(VectorStateID state_id) const {
			auto [__, size, _] = getRootInfo(state_id);

			return size;
		}

		VectorStateID push(VectorStateID state_id, const VarT& var) {
			auto [prev_root, prev_size, height] = getRootInfo(state_id);

			auto node = nodeFromIdxVar(prev_size + 1, var);

			auto prev_path = getNodePath(prev_root, prev_size);
			bool merged    = false;

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				NodeID left = 0, rght = 0;

				if (merged == (dir == Dir::Left)) {
					// when !merged and (dir == R) or merged and (dir == L)
					left = node;
					rght = 0;  // equiv to entry.right
				} else {
					// when merged and (dir == R) or !merged and (dir == L)
					left   = getChild(node_id, Dir::Left);
					rght   = node;
					merged = true;
				}

				node = nodeFromChildren(left, rght);
			}

			if (!merged) {
				node = nodeFromChildren(prev_root, node);
				height++;
			}

			root_info.put(node, RootEntry{ .height = height, .size = prev_size + 1 });

			return VectorStateID{ u64(node) };
		}

		VectorStateID change(VectorStateID state_id, usize idx, const VarT& var) {
			auto [prev_root, size, height] = getRootInfo(state_id);

			if (idx == 0 || idx > size)
				throw std::invalid_argument("received index out of range for given state");

			auto prev_path = getNodePath(prev_root, idx);
			auto node      = nodeFromIdxVar(idx, var);

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				NodeID left = 0, rght = 0;

				if (dir == Dir::Left) {
					left = node;
					rght = getChild(node_id, Dir::Right);
				} else {
					left = getChild(node_id, Dir::Left);
					rght = node;
				}

				node = nodeFromChildren(left, rght);
			}

			root_info.put(node, RootEntry{ .height = height, .size = size });

			return VectorStateID{ u64(node) };
		}

		VectorStateID pop(VectorStateID state_id) {
			auto [prev_var_root, size, height] = getRootInfo(state_id);

			if (size == 0)
				throw std::invalid_argument("stack at given state is empty - cannot pop from it");

			auto prev_path = getNodePath(prev_var_root, size - 1);
			auto last_path = getNodePath(prev_var_root, size);

			std::vector<std::pair<Dir, NodeID>> common = {};

			using namespace std::views;
			for (auto& [f, s]: zip(prev_path, last_path)) {
				if (f != s) break;
				common.emplace_back(f);
			}

			if (common.size() == 0) {
				auto new_root = getChild(prev_var_root, Dir::Left);
				root_info.put(new_root, RootEntry{ .height = height - 1, .size = size - 1 });

				return new_root;
			}

			auto [last_dir, node_id] = common.back();
			NodeID lca               = getChild(node_id, last_dir);

			auto node = nodeFromChildren(getChild(lca, Dir::Left), 0);

			for (auto [dir, node_id]: common | reverse) {
				NodeID left = 0, rght = 0;

				if (dir == Dir::Left) {
					left = node;
					rght = getChild(node_id, Dir::Right);
				} else {
					left = getChild(node_id, Dir::Left);
					rght = node;
				}

				node = nodeFromChildren(left, rght);
			}

			root_info.put(node, RootEntry{ .height = height, .size = size - 1 });

			return VectorStateID{ u64(node) };
		}

		[[nodiscard]]
		VectorStateID getEmpty() const {
			return VectorStateID{ 0 };
		}

		Vector() {
			auto node = NodeID(0);
			root_info.put(node, RootEntry{ .height = 0, .size = 0 });
		}
	};
}
