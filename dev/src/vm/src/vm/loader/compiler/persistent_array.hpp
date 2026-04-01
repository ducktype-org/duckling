#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/loader/compiler/bijective_map.hpp>

#include <ranges>
#include <unordered_set>

namespace persistent {
	STRONG_TYPEDEF_INT(ArrayID, u64);

	template<typename VarT, typename VarH = std::hash<VarT>>
	class Array {
		using NodeID = u64;

		struct NodeEntry {
			NodeID left = 0;
			NodeID rght = 0;

			bool operator==(const NodeEntry&) const = default;
		};

		using NodeEntryH = decltype([](const NodeEntry& h) -> usize {
			return (std::hash<NodeID>{}(h.left) << 1) ^ std::hash<NodeID>{}(h.rght);
		});

		struct LeafEntry {
			usize idx;
			usize val_id;

			bool operator==(const LeafEntry&) const = default;
		};

		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.val_id);
		});

		enum Dir { L, R };

		using Path = std::vector<std::pair<Dir, NodeID>>;

		detail::BijectiveMap<NodeEntry, NodeID, NodeEntryH> node_entries{};
		detail::BijectiveMap<LeafEntry, NodeID, LeafEntryH> leaf_entries{};
		detail::BijectiveMap<VarT, usize, VarH>             held_values{};

		usize                      height = 16;
		std::unordered_set<NodeID> roots;
		NodeID                     next_node_id = 1;

		Path getNodePath(NodeID root, usize idx) {
			Path ans = {};
			Dir  dir = L;

			if (height == 0) return {};

			auto node = root;
			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				if (idx & max_bit) {
					dir  = R;
					node = entry.rght;
				} else {
					dir  = L;
					node = entry.left;
				}
				ans.emplace_back(dir, node);
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
			auto [_, val_id]    = held_values.emplaceByLeft(var, held_values.size());
			auto leaf           = LeafEntry{ .idx = idx, .val_id = val_id };
			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf, next_node_id);
			next_node_id += (is_new ? 1 : 0);

			return node;
		}

		NodeID getChild(NodeID node_id, Dir dir) const {
			auto& entry = node_entries.atRight(node_id);
			return (dir == L) ? entry.left : entry.rght;
		}

	public:
		const VarT& access(ArrayID state_id, usize idx) const {
			auto node = NodeID{ u64(state_id) };

			if (!roots.contains(node))
				throw std::invalid_argument("PersistentVector got invalid state");

			if (idx > (1 << height))
				throw std::invalid_argument("received index out of range for given state");

			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				node        = (idx & max_bit) ? entry.rght : entry.left;
			}

			auto val_id = leaf_entries.atRight(node).val_id;

			return held_values.atRight(val_id);
		}

		ArrayID insert(ArrayID state_id, usize idx, const VarT& var) {
			auto prev_root = NodeID{ u64(state_id) };

			if (!roots.contains(prev_root))
				throw std::invalid_argument("PersistentVector got invalid state");

			auto prev_path = getNodePath(prev_root, idx);
			auto node      = nodeFromIdxVar(idx, var);

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				NodeID left = 0, rght = 0;

				if (dir == L) {
					left = node;
					rght = getChild(node_id, R);
				} else {
					left = getChild(node_id, L);
					rght = node;
				}

				node = nodeFromChildren(left, rght);
			}

			roots.insert(node);

			return ArrayID{ u64(prev_root) };
		}

		ArrayID erase(ArrayID state_id, usize idx) {
			auto prev_root = NodeID{ u64(state_id) };

			if (!roots.contains(prev_root))
				throw std::invalid_argument("PersistentVector got invalid state");

			auto prev_path      = getNodePath(prev_root, idx);
			auto [dir, node_id] = prev_path.back();
			auto node           = getChild(node_id, dir);

			if (node == 0) return state_id;

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				NodeID left = 0, rght = 0;

				if (dir == L) {
					left = node;
					rght = getChild(node_id, R);
				} else {
					left = getChild(node_id, L);
					rght = node;
				}

				node = nodeFromChildren(left, rght);
			}

			roots.insert(node);

			return ArrayID{ u64(prev_root) };
		}

		[[nodiscard]]
		ArrayID getEmpty() const { return ArrayID{ 1 }; }

		Array(usize root_height = 16): height(root_height), next_node_id(2) {
			auto sentinel = NodeID{ 0 };
			auto root     = NodeID{ 1 };

			node_entries.emplaceByLeft(NodeEntry{ .left = sentinel, .rght = sentinel }, sentinel);
			node_entries.emplaceByLeft(NodeEntry{ .left = sentinel, .rght = sentinel }, root);
			roots.emplace(root);
		}
	};
}
