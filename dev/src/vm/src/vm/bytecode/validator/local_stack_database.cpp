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


using ls_db = vm::code::LocalStackDb;

base::Optional<ls_db::NameStackEntry> ls_db::getNameEntryByName(
	StackStateID state, base::StrID name
) const {
	auto name_state_id = validateState(state).first;

	auto entry = namestack_entries.at(name_state_id);

	if (!name_to_namestack.contains(name)) return std::nullopt;

	auto& occurences = name_to_namestack.at(name);
	auto  it         = occurences.lower_bound(entry.lifetime);

	if (it == occurences.end()) return std::nullopt;

	if (auto [deinit, init] = it->first;
	    init > entry.lifetime.deinit_idx || deinit < entry.lifetime.init_idx) {
		return std::nullopt;
	}

	auto val_stack_id = it->second;
	return namestack_entries.at(val_stack_id);
}

base::Optional<ls_db::NameStackEntry> ls_db::getNameEntryByIdx(StackStateID state, usize idx) const {
	auto name_state_id = validateState(state).first;

	if (idx >= nodes_at_depth.size()) return std::nullopt;

	auto  entry      = namestack_entries.at(name_state_id);
	auto& occurences = nodes_at_depth.at(idx);

	auto it = occurences.lower_bound(entry.lifetime);

	if (it == occurences.end()) return std::nullopt;

	auto [deinit, init] = it->first;
	if (init > entry.lifetime.deinit_idx || deinit < entry.lifetime.init_idx) {
		return std::nullopt;
	}

	auto val_stack_id = it->second;
	return namestack_entries.at(val_stack_id);
}

base::Optional<vm::code::valid_type::TypeSize> ls_db::getByteOffset(
	StackStateID state, base::StrID name
) const {
	match_optional(getNameEntryByName(state, name)) {
		opt_some(entry) { return entry.size_in_bytes - entry.size_of_last; }
		opt_none { return std::nullopt; }
	}
	CORE_UNREACHABLE();
}

bool ls_db::contains(StackStateID state, base::StrID name) const {
	return getNameEntryByName(state, name).has_value();
}

base::Optional<usize> ls_db::getIdx(StackStateID state, base::StrID name) const {
	match_optional(getNameEntryByName(state, name)) {
		opt_some(entry) { return entry.size_in_blocks - 1; }
		opt_none { return std::nullopt; }
	}
	CORE_UNREACHABLE();
}

base::Optional<base::StrID> ls_db::getTypeName(StackStateID state, base::StrID name) const {
	match_optional(getIdx(state, name)) {
		opt_some(idx) { return getTypeName(state, idx); }
		opt_none { return std::nullopt; }
	}
	CORE_UNREACHABLE();
}

base::Optional<base::StrID> ls_db::getTypeName(StackStateID state, usize idx) const {
	auto typestack_id = validateState(state).second;

	if (idx >= typestack.size(typestack_id)) return std::nullopt;

	return typestack.at(typestack_id, idx);
}

usize ls_db::size(StackStateID state) const {
	auto name_state_id = validateState(state).first;
	return namestack_entries.at(name_state_id).size_in_blocks;
}

base::Optional<base::StrID> ls_db::getName(StackStateID state, usize idx) const {
	match_optional(getNameEntryByIdx(state, idx)) {
		opt_some(entry) { return entry.name_of_last; }
		opt_none { return std::nullopt; }
	}
	CORE_UNREACHABLE();
}

vm::code::valid_type::TypeSize ls_db::byteSize(StackStateID state) const {
	auto name_state_id = validateState(state).first;
	return namestack_entries.at(name_state_id).size_in_bytes;
}

bool ls_db::eqTypes(StackStateID state_1, StackStateID state_2) const {
	auto typestack_id_1 = validateState(state_1).second;
	auto typestack_id_2 = validateState(state_2).second;

	return typestack.eq(typestack_id_1, typestack_id_2);
}

bool ls_db::eqNames(StackStateID state_1, StackStateID state_2) const {
	auto typestack_id_1 = validateState(state_1).first;
	auto typestack_id_2 = validateState(state_2).first;

	return typestack_id_1 == typestack_id_2;
}

ls_db::LocalStackDb() = default;

/**
 * @brief Constructor serves mostly to check whether passed structures are valid (mostly checks the
 * lifetimes and the relation between nodes in the name tree)
 */
ls_db::LocalStackDb(
	const decltype(name_to_namestack)& name_to_namestack,
	const decltype(namestack_entries)& entries,
	decltype(stack_state_to_substacks) stack_state_to_name_states,
	decltype(typestack)                typestack

):
	  name_to_namestack(name_to_namestack),
	  namestack_entries(entries),
	  stack_state_to_substacks(std::move(stack_state_to_name_states)),
	  typestack(std::move(typestack)) {
	CORE_ASSERT(entries.size(), "we need to have at least one entry (just root)");
	usize             counted_ids = 0;
	std::vector<bool> intervals(entries.size() * 2, false);

	std::vector<usize> number_of_associated(entries.size());
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
			CORE_ASSERT(
				number_of_associated[id] == 0, "There are still slots available for children"
			);
			number_of_associated[id] = 1;
			CORE_ASSERT(entries.at(id).lifetime == lifetime, "keys must match with entries");
		}

		auto it            = maps.begin();
		auto prev_lifetime = it->first;
		++it;

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
	for (auto [id, entry]: enumerate(entries) | drop(1)) {
		CORE_ASSERT(entry.size_in_blocks > 0, "stack is non-empty for each variable");
		nodes_at_depth.at(entry.size_in_blocks - 1).emplace(entry.lifetime, NameStackID(id));
	}

	for (auto& layer: nodes_at_depth) {
		CORE_ASSERT(layer.size(), "each layer must be non-empty");

		auto prev_lifetime = layer.begin()->first;
		for (auto [lifetime, id]: layer | drop(1)) {
			CORE_ASSERT(
				prev_lifetime.deinit_idx < lifetime.init_idx
					|| lifetime.deinit_idx < prev_lifetime.init_idx,
				"in each layer, lifetimes of variables are separate"
			);
			prev_lifetime = lifetime;
		}
	}
}
