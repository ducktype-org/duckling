#pragma once

#include "local_stack_database.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

#include <ranges>
#include <stdexcept>


using ls_db = vm::code::LocalStackDb;

ls_db::NameStackEntry ls_db::getNameEntryByName(StackStateID state, base::StrID name) const {
	auto name_state_id = stack_state_to_substacks.at(u64(state)).first;

	auto entry = namestack_entries.at(name_state_id);

	if (!name_to_namestack.contains(name))
		throw std::invalid_argument("Received completely unknown name");

	auto& occurences = name_to_namestack.at(name);
	auto  it         = occurences.lower_bound(entry.lifetime);

	if (it == occurences.end()) throw std::invalid_argument("No such name at given stack");

	if (auto [init, deinit] = it->first;
	    init > entry.lifetime.deinit_idx || deinit < entry.lifetime.init_idx) {
		throw std::invalid_argument("Variable with given name is not present at the stack instance");
	}

	auto val_stack_id = it->second;
	return namestack_entries.at(val_stack_id);
}

ls_db::NameStackEntry ls_db::getNameEntryByIdx(StackStateID state, usize idx) const {
	auto name_state_id = stack_state_to_substacks.at(u64(state)).first;

	auto  entry      = namestack_entries.at(name_state_id);
	auto& occurences = nodes_at_depth.at(idx);

	auto it = occurences.lower_bound(entry.lifetime);

	if (auto [init, deinit] = it->first;
	    init > entry.lifetime.deinit_idx || deinit < entry.lifetime.init_idx) {
		throw std::invalid_argument("Variable with given name is not present at the stack instance");
	}

	auto val_stack_id = it->second;
	return namestack_entries.at(val_stack_id);
}

ls_db::tp_size ls_db::getByteOffset(StackStateID state, base::StrID name) const {
	auto entry = getNameEntryByName(state, name);
	return entry.size_in_bytes - entry.size_of_last;
}

bool ls_db::contains(StackStateID state, base::StrID name) const {
	try {
		getNameEntryByName(state, name);
		return true;
	} catch (std::invalid_argument& e) { return false; }
}

usize ls_db::getIdx(StackStateID state, base::StrID name) const {
	return getNameEntryByName(state, name).size_in_blocks - 1;
}

base::StrID ls_db::getTypeName(StackStateID state, base::StrID name) const {
	auto idx = getIdx(state, name);
	return getTypeName(state, idx);
}

base::StrID ls_db::getTypeName(StackStateID state, usize idx) const {
	auto typestack_id = stack_state_to_substacks.at(u64(state)).second;
	return typestack.at(typestack_id, idx);
}

usize ls_db::size(StackStateID state) const {
	auto name_state_id = stack_state_to_substacks.at(u64(state)).first;
	return namestack_entries.at(name_state_id).size_in_blocks;
}

base::StrID ls_db::getName(StackStateID state, usize idx) const {
	auto entry = getNameEntryByIdx(state, idx);
	return entry.name_of_last;
}

bool ls_db::eqTypes(StackStateID state_1, StackStateID state_2) {
	auto typestack_id_1 = stack_state_to_substacks.at(u64(state_1)).second;
	auto typestack_id_2 = stack_state_to_substacks.at(u64(state_2)).second;

	return typestack.eq(typestack_id_1, typestack_id_2);
}

ls_db::LocalStackDb() = default;

ls_db::LocalStackDb(
	const decltype(name_to_namestack)&        name_to_namestack,
	const decltype(namestack_entries)&        entries,
	const decltype(stack_state_to_substacks)& stack_state_to_name_states,
	const decltype(typestack)&                typestack

):
	  name_to_namestack(name_to_namestack),
	  namestack_entries(entries),
	  stack_state_to_substacks(stack_state_to_name_states),
	  typestack(typestack) {
	CORE_ASSERT(entries.size(), "we need to have at least one entry (just root)");
	usize             counted_ids = 0;
	std::vector<bool> intervals(entries.size() * 2, false);

	std::vector<usize> number_of_children(entries.size());
	using namespace std::views;

	{
		auto [r0, l0] = entries.at(0).lifetime;
		CORE_ASSERT(l0 == 0 && r0 == entries.size() * 2 - 1, "root has a trivial lifetime");
		intervals.at(l0) = true;
		intervals.at(r0) = true;
	}

	usize max_depth = 0;

	for (const auto& [id, curr_entry]: enumerate(entries) | drop(1)) {
		max_depth = std::max(max_depth, curr_entry.size_in_blocks);

		auto prev_entry_id = curr_entry.prev;
		CORE_ASSERT(prev_entry_id < id, "Previous state must have been created before current");
		number_of_children[prev_entry_id]++;
		const auto& prev_entry = entries[prev_entry_id];

		auto [curr_r, curr_l] = curr_entry.lifetime;
		CORE_ASSERT(
			curr_l < intervals.size() && curr_r < intervals.size(),
			"lifetime idx is limited by number of states"
		);
		CORE_ASSERT(curr_l < curr_r, "init idx must be smaller than deinit idx");
		CORE_ASSERT(!intervals.at(curr_l) && !intervals.at(curr_r), "All interval ends are unique");
		intervals.at(curr_l) = true;
		intervals.at(curr_r) = true;

		auto [prev_r, prev_l] = prev_entry.lifetime;
		CORE_ASSERT(
			prev_l < curr_l && curr_r < prev_r,
			"previous variable was initialized before current and deinitialized after"
		);

		static constexpr auto PTR_SIZE_1 = Bytes{ 8ull };
		static constexpr auto PTR_SIZE_2 = Bytes{ 16ull };

		auto byte_size_1 = curr_entry.size_in_bytes.assumePointerSize(PTR_SIZE_1);
		auto byte_size_2 = curr_entry.size_in_bytes.assumePointerSize(PTR_SIZE_2);

		auto prev_byte_size_1 = prev_entry.size_in_bytes.assumePointerSize(PTR_SIZE_1);
		auto prev_byte_size_2 = prev_entry.size_in_bytes.assumePointerSize(PTR_SIZE_2);

		CORE_ASSERT(prev_byte_size_1 < byte_size_1, "variables have non-zero size");
		CORE_ASSERT(prev_byte_size_2 < byte_size_2, "variables have non-zero size");

		CORE_ASSERT(
			prev_entry.size_in_blocks + 1 == curr_entry.size_in_blocks,
			"current entry has one block more the previous"
		);
	}


	for (const auto& [name, maps]: name_to_namestack) {
		counted_ids += maps.size();

		CORE_ASSERT(maps.size(), "Each name must have at least correlated namestack state");

		for (auto& [lifetime, id]: maps) {
			CORE_ASSERT(id < entries.size(), "id has to point to valid name-stack entry");
			CORE_ASSERT(number_of_children[id] != 0, "There are still slots available for children");
			number_of_children[id]--;
			CORE_ASSERT(entries.at(id).lifetime == lifetime, "keys must match with entries");
		}

		auto it            = maps.begin();
		auto prev_lifetime = it->first;

		for (; it != maps.end(); ++it) {
			CORE_ASSERT(
				prev_lifetime.deinit_idx < it->first.init_idx,
				"previous variable (with the same name) must be deinitilized before we "
				"initilize current"
			);
		}
	}

	CORE_ASSERT(counted_ids + 1 == entries.size(), "We have a equal number of ids and ");

	nodes_at_depth.resize(max_depth, {});
	for (auto [id, entry]: enumerate(entries))
		nodes_at_depth.at(entry.size_in_blocks - 1).emplace(entry.lifetime, NameStackID(id));
}

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

		tree.emplace_back(
			TreeNode{
				.children      = {},
				.name_map_id   = new_namemap_id,
				.name_stack_id = new_varname_id,
				.size          = tree[node_id].size + 1,
				.prev_node     = node_id,
				.byte_depth    = tree[node_id].byte_depth + types_ctx.at(type)->getSize(),
			}
		);
	}

	auto new_typestack_state_id = typenames.push(typestack_id, type);

	states.emplace_back(new_nametree_node_id, new_typestack_state_id);

	return StackStateID{ states.size() - 1 };
}

vm::code::StackStateID ls_db_bld::pop(StackStateID state, usize amount) {
	auto [node_id, typestack_id] = states.at(u64{ state });

	auto new_nametree_node_id = node_id;
	for (usize i = 0; i < amount; i++) {
		CORE_ASSERT(new_nametree_node_id != 0, "We are not trying to remove the root");
		new_nametree_node_id = tree[new_nametree_node_id].prev_node;
	}

	auto new_typestack_state_id = typenames.pop(typestack_id, amount);

	states.emplace_back(new_nametree_node_id, new_typestack_state_id);

	return StackStateID{ states.size() - 1 };
}

vm::code::StackStateID ls_db_bld::change(StackStateID state, base::StrID name, base::StrID type) {
	auto [node_id, typestack_id] = states.at(u64{ state });

	auto name_map_id            = tree[node_id].name_map_id;
	auto idx                    = name_to_idx.at(name_map_id, name);
	auto new_typestack_state_id = typenames.change(typestack_id, idx, type);

	states.emplace_back(node_id, new_typestack_state_id);

	return StackStateID{ states.size() - 1 };
}

base::StrID ls_db_bld::typeOf(StackStateID state, usize idx) const {
	auto [node_id, typestack_id] = states.at(u64{ state });
	CORE_ASSERT(idx < typenames.size(typestack_id), "idx should be valid");

	return typenames.at(typestack_id, idx);
}


base::StrID ls_db_bld::typeOf(StackStateID state, base::StrID name) const {
	auto [node_id, typestack_id] = states.at(u64{ state });
	auto name_map_id             = tree[node_id].name_map_id;

	if (!name_to_idx.contains(name_map_id, name))
		throw std::invalid_argument("No such variable at given instance");

	auto idx = name_to_idx.at(name_map_id, name);

	return typeOf(state, idx);
}

usize ls_db_bld::size(StackStateID state) const {
	auto [node_id, typestack_id] = states.at(u64{ state });
	return tree[node_id].size;
}

bool ls_db_bld::contains(StackStateID state, base::StrID name) const {
	auto [node_id, typestack_id] = states.at(u64{ state });
	auto name_map_id             = tree[node_id].name_map_id;

	return name_to_idx.contains(name_map_id, name);
}

base::StrID ls_db_bld::getName(StackStateID state, usize idx) const {
	auto [node_id, typestack_id] = states.at(u64{ state });

	CORE_ASSERT(idx < tree[node_id].size, "We need this to be true for name retrieval");
	auto varname_stack_id = tree[node_id].name_stack_id;

	return var_names.at(varname_stack_id, idx);
}

vm::code::LocalStackDb ls_db_bld::finalize() {
	usize order = 0;

	base::HashMap<base::StrID, NameMap>              name_to_namestack_id{};
	std::vector<NameStackEntry>                      namestack_entries(tree.size());
	std::vector<std::pair<NameStackID, TypeStackID>> stack_state_to_substacks{ states.size() };


	auto dfs = [&](auto&&                                   self,
	               NameStackID                              node_id,
	               base::HashMap<base::StrID, NameStackID>& names) -> Lifetime {
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


	return LocalStackDb(
		name_to_namestack_id, namestack_entries, stack_state_to_substacks, typenames
	);
}
