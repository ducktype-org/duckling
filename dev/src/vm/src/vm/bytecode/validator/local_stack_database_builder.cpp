// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "local_stack_database_builder.hpp"

using ls_db_bld = vm::code::LocalStackDbBuilder;

ls_db_bld::NameStackID ls_db_bld::TreeNode::emplaceChild(const Child& child, NameStackID new_id) {
	if_opt_some(children.atMaybeCopy(child), node_id) return node_id;

	children.put(child, new_id);
	return new_id;
}

vm::code::StackStateID ls_db_bld::push(StackStateID state, base::StrID name, base::StrID type) {
	auto [node_id, typestack_id] = states.at(u64{ state });

	auto child = Child{
		.name        = name,
		.byte_offset = tree[node_id].byte_depth + types_ctx.at(type)->getSize(),
	};

	auto new_nametree_node_id = tree[node_id].emplaceChild(child, tree.size());

	if (new_nametree_node_id == tree.size()) {
		auto name_map_id    = tree[node_id].name_map_id;
		auto new_namemap_id = name_to_idx.insert(name_map_id, name, tree[node_id].size);
		auto var_name_id    = tree[node_id].name_stack_id;
		auto new_varname_id = var_names.push(var_name_id, name);

		tree.emplace_back(TreeNode{
			.children      = {},
			.name_map_id   = new_namemap_id,
			.name_stack_id = new_varname_id,
			.size          = tree[node_id].size + 1,
			.prev_node     = node_id,
			.byte_depth    = tree[node_id].byte_depth + types_ctx.at(type)->getSize(),
		});
	}

	auto new_typestack_state_id = typenames.push(typestack_id, type);

	states.emplace_back(new_nametree_node_id, new_typestack_state_id);

	return StackStateID{ states.size() - 1 };
}

vm::code::StackStateID ls_db_bld::pop(StackStateID state, usize amount) {
	auto [node_id, typestack_id] = states.at(u64{ state });

	auto start_size = size(state);

	auto new_nametree_node_id = node_id;
	for (usize i = 0; i < amount; i++) {
		CORE_ASSERT(new_nametree_node_id != 0, "We are not trying to remove the root");
		new_nametree_node_id = tree[new_nametree_node_id].prev_node;
	}

	auto new_typestack_state_id = typenames.pop(typestack_id, amount);

	states.emplace_back(new_nametree_node_id, new_typestack_state_id);

	auto ans = StackStateID{ states.size() - 1 };
	CORE_ASSERT(size(ans) + amount == start_size, "We popped the values");

	return ans;
}

vm::code::StackStateID ls_db_bld::change(StackStateID state, base::StrID name, base::StrID type) {
	auto [node_id, typestack_id] = validateState(state);

	auto name_map_id            = tree[node_id].name_map_id;
	auto idx                    = name_to_idx.at(name_map_id, name);
	auto new_typestack_state_id = typenames.change(typestack_id, idx, type);

	states.emplace_back(node_id, new_typestack_state_id);

	return StackStateID{ states.size() - 1 };
}

base::Optional<base::StrID> ls_db_bld::getTypeName(StackStateID state, usize idx) const {
	auto [node_id, typestack_id] = validateState(state);

	if (idx >= typenames.size(typestack_id)) return std::nullopt;

	return typenames.at(typestack_id, idx);
}

base::Optional<base::StrID> ls_db_bld::getTypeName(StackStateID state, base::StrID name) const {
	auto node_id     = validateState(state).first;
	auto name_map_id = tree[node_id].name_map_id;

	if (!name_to_idx.contains(name_map_id, name)) return std::nullopt;

	auto idx = name_to_idx.at(name_map_id, name);

	return getTypeName(state, idx);
}

usize ls_db_bld::size(StackStateID state) const {
	auto node_id = validateState(state).first;
	return tree[node_id].size;
}

bool ls_db_bld::contains(StackStateID state, base::StrID name) const {
	auto node_id     = validateState(state).first;
	auto name_map_id = tree[node_id].name_map_id;

	return name_to_idx.contains(name_map_id, name);
}

bool ls_db_bld::eqTypes(StackStateID state_1, StackStateID state_2) const {
	auto typestack_id_1 = validateState(state_1).second;
	auto typestack_id_2 = validateState(state_2).second;

	return typenames.eq(typestack_id_1, typestack_id_2);
}

bool ls_db_bld::eqNames(StackStateID state_1, StackStateID state_2) const {
	auto node_id_1 = validateState(state_1).first;
	auto node_id_2 = validateState(state_2).first;

	auto name_map_id_1 = tree[node_id_1].name_map_id;
	auto name_map_id_2 = tree[node_id_2].name_map_id;

	return name_to_idx.eq(name_map_id_1, name_map_id_2);
}

base::Optional<base::StrID> ls_db_bld::getName(StackStateID state, usize idx) const {
	auto [node_id, typestack_id] = validateState(state);

	if (idx >= tree[node_id].size) return std::nullopt;

	auto varname_stack_id = tree[node_id].name_stack_id;

	return var_names.at(varname_stack_id, idx);
}

vm::code::LocalStackDb ls_db_bld::finalize() {
	usize order = 0;

	base::HashMap<base::StrID, NameMap> name_to_namestack_id{};
	std::vector<NameStackEntry>         namestack_entries(tree.size());


	auto dfs = [&](auto&& self, NameStackID node_id, base::HashMap<base::StrID, NameStackID>& names
	           ) -> Lifetime {
		CORE_ASSERT(node_id < tree.size(), "this should be true");
		Lifetime node_lifetime{};
		node_lifetime.init_idx = order;
		order++;

		for (auto& [child, next_node]: tree[node_id].children) {
			CORE_ASSERT(!names.contains(child.name), "we should never have repeating names");

			names.emplace(child.name, next_node);

			auto next_lifetime = self(self, next_node, names);

			name_to_namestack_id.put(child.name, {});
			name_to_namestack_id.at(child.name).emplace(next_lifetime, next_node);

			namestack_entries.at(next_node) = NameStackEntry{
				.lifetime       = next_lifetime,
				.prev           = node_id,
				.size_in_bytes  = tree[next_node].byte_depth,
				.size_in_blocks = tree[next_node].size,
				.size_of_last   = tree[next_node].byte_depth - tree[node_id].byte_depth,
				.name_of_last   = child.name,
			};

			names.erase(child.name);
		}

		node_lifetime.deinit_idx = order;
		order++;


		return node_lifetime;
	};

	base::HashMap<base::StrID, NameStackID> names = {};

	auto root_lifetime = dfs(dfs, 0, names);

	namestack_entries.at(0) = NameStackEntry{
		.lifetime = root_lifetime,
	};


	return LocalStackDb{ name_to_namestack_id, namestack_entries, states, typenames };
}
