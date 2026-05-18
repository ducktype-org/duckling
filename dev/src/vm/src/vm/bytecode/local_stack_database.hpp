#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>
#include <ranges>

namespace persistent {
	template<typename VarT, typename ValT>
	class LocalStackDbBuilder;

	template<typename VarT, typename ValT>
	class LocalStackDb;

	STRONG_TYPEDEF_INT(StackStateID, u64);

	template<typename VarT, typename ValT>
	class LocalStackDb {
		friend LocalStackDbBuilder<VarT, ValT>;

		//// Vals - immutable parts (usually decltype info)
		using ValNodeID = u64;

		struct Lifetime {
			usize deinit_idx                         = 0;
			usize init_idx                           = 0;
			auto  operator<=>(const Lifetime&) const = default;
		};

		struct ValNode {
			Lifetime  lifetime{};
			ValNodeID prev = 0;
			usize     size = 0;
			ValT      info{};
		};

		using ValsMap = std::map<Lifetime, ValNodeID>;

		// Vars - values which can be changed in
		using VarNodeID = u64;

		struct VarNode {
			VarNodeID left;
			VarNodeID rght;
		};

		using VarNodeH = decltype([](const VarNode& h) -> usize {
			return (std::hash<VarNodeID>{}(h.left) << 1) ^ std::hash<VarNodeID>{}(h.rght);
		});

		struct StackState {
			ValNodeID val_state;
			VarNodeID var_state;
		};

		using StackStateH = decltype([](const StackState& s) -> usize {
			return (std::hash<ValNodeID>{}(s.val_state) << 1) ^ std::hash<VarNodeID>{}(s.var_state);
		});

		base::HashMap<ValNodeID, ValNode>   val_entries;
		base::HashMap<base::StrID, ValsMap> name_to_decl_info;

		base::HashMap<VarNodeID, VarNode> var_entries;
		base::HashMap<VarNodeID, usize>   root_heights;
		base::HashMap<VarNodeID, VarT>    leaf_values;

		base::HashMap<StackStateID, StackState> states;

		LocalStackDb(
			decltype(val_entries)       values_entry,
			decltype(name_to_decl_info) name_to_decl_info,
			decltype(var_entries)       var_entries,
			decltype(root_heights)      root_height,
			decltype(leaf_values)       leaf_values,
			decltype(states)            states
		):
			  val_entries(std::move(values_entry)),
			  name_to_decl_info(std::move(name_to_decl_info)),
			  var_entries(std::move(var_entries)),
			  root_heights(std::move(root_height)),
			  leaf_values(std::move(leaf_values)),
			  states(std::move(states)) {}

	public:
		LocalStackDb() = default;
	};

	template<typename VarT, typename ValT>
	class LocalStackDbBuilder {
		using Prod = LocalStackDb<VarT, ValT>;

		using ValNodeID = Prod::ValNodeID;
		using Lifetime  = Prod::Lifetime;
		using ValNode   = Prod::ValNode;
		using ValsMap   = Prod::ValsMap;

		using VarNodeID  = Prod::VarNodeID;
		using VarNode    = Prod::VarNode;
		using StackState = Prod::StackState;

		using ValEntries   = decltype(Prod::val_entries);
		using NameDeclInfo = decltype(Prod::name_to_decl_info);
		using VarEntries   = decltype(Prod::var_entries);
		using RootHeights  = decltype(Prod::root_heights);
		using LeafValues   = decltype(Prod::leaf_values);
		using States       = decltype(Prod::states);

		using VarNodeH    = Prod::VarNodeH;
		using StackStateH = Prod::StackStateH;

		ValEntries   val_entries;
		NameDeclInfo name_to_decl_info;
		VarEntries   var_entries;
		RootHeights  root_heights;
		LeafValues   leaf_values;
		States       states;

		struct NameVal {
			base::StrID name;
			ValT        val;
		};

		using NameValH = decltype([](const NameVal& h) -> usize {
			return (std::hash<base::StrID>{}(h.name) << 1) ^ (std::hash<ValT>{}(h.val));
		});

		using ValChildren = base::HashMap<NameVal, ValNodeID, NameValH>;

		base::HashMap<ValNodeID, ValChildren>                children;
		base::HashMap<VarNode, VarNodeID, VarNodeH>          children_to_node;
		base::HashMap<usize, base::HashMap<VarT, VarNodeID>> prev_vars;
		base::HashMap<StackState, StackStateID, StackStateH> state_to_id;

		enum Dir { L, R };

		std::vector<std::pair<Dir, VarNodeID>> getNodePath(VarNodeID root, usize idx) {
			if (root == 0) return {};

			std::vector<std::pair<Dir, VarNodeID>> ans    = {};
			Dir                                    dir    = L;
			auto                                   height = root_heights[root];

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

		VarNodeID varNodeFromChildren(VarNodeID left, VarNodeID rght) {
			auto children = VarNode{ .left = left, .rght = rght };
			auto [_, it]  = children_to_node.put(children, var_entries.size());

			auto var_node = it->second;
			var_entries.put(var_node, children);

			return var_node;
		}

		VarNodeID varNodeFromIdxCons(usize idx, VarT var) {
			prev_vars.put(idx, {});
			auto [_, var_it] = prev_vars[idx].put(var, var_entries.size());

			return var_it->second;
		}

		ValNodeID valNodeFromPrevAndCons(ValNodeID prev, base::StrID name, ValT val) {
			auto prev_size = val_entries[prev].size;
			auto nameval   = NameVal{ .name = std::move(name), .val = std::move(val) };

			children.put(prev, {});
			auto [_, val_it] = children[prev].put(nameval, val_entries.size());

			ValNodeID val_node = val_it->second;
			val_entries.put(val_node, ValNode{ .info = val, .prev = prev, .size = prev_size + 1 });

			return val_node;
		}

		StackStateID getStateID(VarNodeID var_node, ValNodeID val_node) {
			auto new_state            = StackState{ .var_state = var_node, .val_state = val_node };
			auto [_, new_state_id_it] = state_to_id.emplace(new_state, states.size());

			return new_state_id_it->second;
		}

		VarNodeID getSon(VarNodeID node_id, Dir dir) const {
			auto& entry = var_entries[node_id];
			return (dir == L) ? entry.left : entry.rght;
		}

	public:
		StackStateID push(StackStateID state_id, base::StrID name, ValT val, VarT var) {
			if (!states.contains(state_id))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto& state         = states[state_id];
			auto  prev_var_root = state.var_state;
			auto  prev_val_node = state.val_state;
			auto  prev_size     = val_entries[prev_val_node].size;

			ValNodeID val_node
				= valNodeFromPrevAndCons(prev_val_node, std::move(name), std::move(val));

			auto var_node = varNodeFromIdxCons(prev_size + 1, std::move(var));

			auto prev_path = getNodePath(prev_var_root, prev_size);
			bool merged    = false;

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				VarNodeID left, rght = 0;

				if (merged == (dir == L)) {
					// when !merged and (dir == R) or merged and (dir == L)
					left = var_node;
					rght = 0;  // equiv to entry.right
				} else {
					// when merged and (dir == R) or !merged and (dir == L)
					left   = getSon(node_id, L);
					rght   = var_node;
					merged = true;
				}

				var_node = varNodeFromChildren(left, rght);
			}

			if (!merged) var_node = varNodeFromChildren(prev_var_root, var_node);

			return getStateID(var_node, val_node);
		}

		StackStateID change(StackStateID state_id, usize idx, VarT var) {
			if (!states.contains(state_id))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto& state         = states[state_id];
			auto  prev_var_root = state.var_state;
			auto  val_node      = state.val_state;
			auto  size          = val_entries[val_node].size;

			if (idx == 0 || idx > size)
				throw std::invalid_argument("received index out of range for given state");

			auto prev_path = getNodePath(prev_var_root, idx);
			auto var_node  = varNodeFromIdxCons(idx, std::move(var));

			using namespace std::views;
			for (auto [dir, node_id]: prev_path | reverse) {
				VarNodeID left, rght;

				if (dir == L) {
					left = var_node;
					rght = getSon(node_id, R);
				} else {
					left = getSon(node_id, L);
					rght = var_node;
				}

				var_node = varNodeFromChildren(left, rght);
			}

			return getStateID(var_node, val_node);
		}

		StackStateID pop(StackStateID state_id) {
			if (!states.contains(state_id))
				throw std::invalid_argument("LocalStackDbBuilder got invalid state");

			auto& state         = states[state_id];
			auto  prev_var_root = state.var_state;
			auto  val_node      = state.val_state;
			auto  size          = val_entries[val_node].size;
			auto  prev_val_node = val_entries[val_node].prev;

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

			if (common.size() == 0) return getSon(prev_var_root, L);

			auto [last_dir, node_id] = common.back();
			VarNodeID lca            = getSon(node_id, last_dir);

			auto var_node = varNodeFromChildren(getSon(lca, L), 0);

			for (auto [dir, node_id]: common | reverse) {
				VarNodeID left, rght;

				if (dir == L) {
					left = var_node;
					rght = getSon(node_id, R);
				} else {
					left = getSon(node_id, L);
					rght = var_node;
				}

				var_node = varNodeFromChildren(left, rght);
			}

			return getStateID(var_node, prev_val_node);
		}
	};
}
