#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>

#include <vm/utils/bijective_map.hpp>

#include <ranges>
#include <stdexcept>
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

		static constexpr auto SENTINEL = NodeID{ 0 };

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

		NodeID getNodeAt(NodeID root, usize height, usize idx) {
			auto node = root;
			for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
				auto& entry = node_entries.atRight(node);
				node        = (idx & max_bit) ? entry.rght : entry.left;
			}
			return node;
		}

		void advancePath(Path& path) const {
			const auto orig_size = path.size();

			while (path.size() && path.back().first == Dir::Left) path.pop_back();

			CORE_ASSERT(path.size(), "We require that at least node on path is left son");

			NodeID node       = path.back().second;
			path.back().first = Dir::Right;

			while (path.size() < orig_size) {
				node = getChild(node, Dir::Right);
				path.emplace_back(Dir::Right, node);
			}
		}

		const VarT& getLeafValue(NodeID node) {
			auto var_id = leaf_entries.atRight(node).var_id;

			return held_values.atRight(var_id);
		}

	public:
		const VarT& access(VectorStateID state_id, usize idx) const {
			auto [root, size, height] = getRootInfo(state_id);

			if (idx == 0 || idx > size)
				throw std::invalid_argument("received index out of range for given state");
			CORE_ASSERT(height > 0, "Root of non-empty vector should always have non-zero height");

			auto node = getNodeAt(root, height, idx);

			return getLeafValue(node);
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
					rght = SENTINEL;  // equiv to entry.right
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

		std::vector<VarT> view(VectorStateID state_id, usize left, usize right) {
			auto [prev_root, size, root_height] = getRootInfo(state_id);

			if (right < left) throw std::invalid_argument("right has to be bigger-equal than left");
			if (right > size)
				throw std::invalid_argument("trying to take view which is outside of size");

			if (right == 0) return {};
			if (left == 0) left = 1;

			std::vector<VarT> ans = {};

			auto path = getNodePath(prev_root, left);

			for (usize idx = left; idx <= right; idx++) {

				NodeID leaf;
				{
					auto [dir, last] = path.back();
					leaf = getChild(last, dir);
				}

				ans.emplace_back(getLeafValue(leaf));

				if (idx == right) break;

				advancePath(path);
			}

			return ans;
		}

		VectorStateID take(VectorStateID state_id, usize prefix_size) {
			auto [prev_root, size, root_height] = getRootInfo(state_id);

			if (prefix_size == 0) return VectorStateID{ SENTINEL };
			if (prefix_size == size) return state_id;

			if (prefix_size > size)
				throw std::invalid_argument("Trying to take more elements than are in vector");

			usize new_height = 0;
			for (usize exp = 1; exp <= prefix_size; exp *= 2, new_height++);

			using namespace std::views;
			auto path = getNodePath(prev_root, prefix_size) | reverse | take(new_height)
			          | std::ranges::to<std::vector>;

			auto node = getChild(path[0].second, path[0].first);

			for (auto [dir, node_id]: path) {
				NodeID left = 0, rght = 0;

				if (dir == Dir::Left) {
					left = node;
					rght = SENTINEL;
				} else {
					left = getChild(node_id, Dir::Left);
					rght = node;
				}

				node = nodeFromChildren(left, rght);
			}

			root_info.put(node, RootEntry{ .height = new_height, .size = prefix_size });

			return VectorStateID{ node };
		}

		VectorStateID pop(VectorStateID state_id, usize how_many_pop = 1) {
			auto [prev_var_root, size, height] = getRootInfo(state_id);

			if (size < how_many_pop)
				throw std::invalid_argument("stack at given state is empty - cannot pop from it");

			return take(state_id, size - how_many_pop);
		}

		std::vector<VarT> slice(VectorStateID state_id, usize idx_L, usize idx_R) {
			auto [prev_var_root, size, height] = getRootInfo(state_id);
		}

		[[nodiscard]]
		VectorStateID getEmpty() const {
			return VectorStateID{ SENTINEL };
		}

		Vector() {
			root_info.put(SENTINEL, RootEntry{ .height = 0, .size = 0 });
			node_entries.addLink(NodeEntry{ .left = SENTINEL, .rght = SENTINEL }, SENTINEL);
		}
	};
}
