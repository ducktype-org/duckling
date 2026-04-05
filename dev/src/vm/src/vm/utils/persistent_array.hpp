#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <ranges>
#include <stdexcept>
#include <unordered_set>

namespace persistent {
	STRONG_TYPEDEF_INT(ArrayStateID, u64);

	/**
	 * @brief A persistent data structure, which simulates array. It can store up to 2^{root_height}
	 * elements.
	 * @note Implementation based of persistent segment tree.
	 * @note Held values are constructed only once, and nodes hold their id's. This is to allow for
	 * quick construction of leaf elements and to avoid any assumptions about the hash function of
	 * values.
	 *
	 * @tparam VarT
	 * @tparam VarH
	 */
	template<typename VarT, typename VarH = std::hash<VarT>>
	class Array {
		using NodeID = u64;

		static constexpr auto SENTINEL  = NodeID{ 0 };
		static constexpr auto ORIG_ROOT = NodeID{ 1 };

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

		enum class Dir { Left, Right };

		using Path = std::vector<std::pair<Dir, NodeID>>;

		detail::BijectiveMap<NodeEntry, NodeID, NodeEntryH> node_entries{};
		detail::BijectiveMap<LeafEntry, NodeID, LeafEntryH> leaf_entries{};
		detail::BijectiveMap<VarT, usize, VarH>             held_values{};

		usize                      height = 16;
		std::unordered_set<NodeID> roots;
		NodeID                     next_node_id = 1;

		Path getNodePath(NodeID root, usize idx) {
			Path ans = {};
			Dir  dir = Dir::Left;

			if (height == 0) return {};

			auto node = root;
			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				if (idx & max_bit) {
					dir  = Dir::Right;
					node = entry.rght;
				} else {
					dir  = Dir::Left;
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
			return (dir == Dir::Left) ? entry.left : entry.rght;
		}

		NodeID getRootAndValidateIdx(ArrayStateID state_id, usize idx) {
			auto node = NodeID{ u64(state_id) };

			if (!roots.contains(node))
				throw std::invalid_argument("PersistentVector got invalid state");

			if (idx > (1 << height))
				throw std::invalid_argument("received index out of range for given state");

			return node;
		}

		NodeID getNodeAt(NodeID root, usize idx) {
			auto node = root;
			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				node        = (idx & max_bit) ? entry.rght : entry.left;
			}
			return node;
		}

	public:
		bool active(ArrayStateID state_id, usize idx) {
			auto root = getRootAndValidateIdx(state_id, idx);
			auto node = getNodeAt(root, idx);

			return (node != SENTINEL);
		}

		const VarT& access(ArrayStateID state_id, usize idx) const {
			auto root = getRootAndValidateIdx(state_id, idx);
			auto node = getNodeAt(root, idx);

			if (node == SENTINEL)
				throw std::invalid_argument("At given position, there is no value");

			auto val_id = leaf_entries.atRight(node).val_id;

			return held_values.atRight(val_id);
		}

		ArrayStateID insert(ArrayStateID state_id, usize idx, const VarT& var) {
			auto prev_root = getRootAndValidateIdx(state_id, idx);

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

			roots.insert(node);

			return ArrayStateID{ u64(prev_root) };
		}

		std::pair<bool, ArrayStateID> emplace(ArrayStateID state_id, usize idx, const VarT& var) {
			auto prev_root = getRootAndValidateIdx(state_id, idx);

			auto prev_path      = getNodePath(prev_root, idx);
			auto [dir, node_id] = prev_path.back();
			auto prev_node      = getChild(node_id, dir);

			if (prev_node != SENTINEL) return { false, state_id };

			auto node = nodeFromIdxVar(idx, var);
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

			roots.insert(node);

			return { true, ArrayStateID{ u64(node) } };
		}

		ArrayStateID erase(ArrayStateID state_id, usize idx) {
			auto prev_root = getRootAndValidateIdx(state_id, idx);

			auto prev_path      = getNodePath(prev_root, idx);
			auto [dir, node_id] = prev_path.back();
			auto node           = getChild(node_id, dir);

			if (node == SENTINEL) return state_id;

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

			roots.insert(node);

			return ArrayStateID{ u64(prev_root) };
		}

		[[nodiscard]]
		ArrayStateID getEmpty() const {
			return ArrayStateID{ u64(ORIG_ROOT) };
		}

		Array(usize root_height = 16): height(root_height), next_node_id(2) {
			node_entries.emplaceByLeft(NodeEntry{ .left = SENTINEL, .rght = SENTINEL }, SENTINEL);
			node_entries.emplaceByLeft(NodeEntry{ .left = SENTINEL, .rght = SENTINEL }, ORIG_ROOT);
			roots.emplace(ORIG_ROOT);
		}
	};
}
