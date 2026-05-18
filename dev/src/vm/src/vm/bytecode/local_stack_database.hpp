#pragma once

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

#include <functional>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <variant>

class LocalStackDbBuilder;
STRONG_TYPEDEF_INT(StackStateID, u64);

namespace vm::persistent {

	template<typename T>
	class DummyVector {
		std::vector<std::pair<base::Optional<usize>, std::vector<T>>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We have a copy of given state");
			return copies.at(state);
		}

	public:
		[[nodiscard]]
		usize pop(usize state) {
			auto& curr_state = validateState(state);

			if_opt_some(curr_state.first, prev) return prev;

			auto copy = curr_state.second;
			copy.pop_back();
			copies.emplace_back(std::nullopt, copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		usize change(usize state, usize idx, const T& val) {
			auto copy    = validateState(state).second;
			copy.at(idx) = val;
			copies.emplace_back(std::nullopt, copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		usize push(usize state, const T& val) {
			auto copy = validateState(state).second;
			copy.push_back(val);
			copies.emplace_back(state, copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1).second == validateState(state_2).second;
		}

		[[nodiscard]]
		const T& at(usize state, usize idx) const {
			return validateState(state).second.at(idx);
		}

		DummyVector<T>(): copies({ std::make_pair(base::Optional<usize>{}, std::vector<T>{}) }) {}
	};
}

namespace vm::code {

	class LocalStackDbBuilder;

	class LocalStackDb {
		friend LocalStackDbBuilder;
		using tp_size = valid_type::TypeSize;

		using NameStackID = u64;
		using TypeStackID = u64;

		struct Lifetime {
			usize deinit_idx                         = 0;
			usize init_idx                           = 0;
			auto  operator<=>(const Lifetime&) const = default;
		};

		struct NameStackEntry {
			Lifetime    lifetime{};
			NameStackID prev = 0;
			tp_size     size_in_bytes{};
			usize       size_in_blocks{};
		};

		NameStackEntry getNameEntry(StackStateID state, base::StrID name) const {
			auto name_state_id = stack_state_to_substacks.at(u64(state)).first;

			auto& occurences = name_to_namestack_id.at(name);
			auto  entry      = namestack_entries.at(name_state_id);

			auto it = occurences.lower_bound(entry.lifetime);

			if (it == occurences.end()) throw std::invalid_argument("No such name at given stack");

			if (auto [init, deinit] = it->first;
			    init > entry.lifetime.deinit_idx || deinit < entry.lifetime.init_idx) {
				throw std::invalid_argument(
					"Variable with given name is not present at the stack instance"
				);
			}

			auto val_stack_id = it->second;
			return namestack_entries.at(val_stack_id);
		}

	public:
		tp_size getByteOffset(StackStateID state, base::StrID name) const {
			return getNameEntry(state, name).size_in_bytes;
		}

		usize getIdxOf(StackStateID state, base::StrID name) const {
			return getNameEntry(state, name).size_in_blocks;
		}

		auto getType(StackStateID state, base::StrID name) const {
			auto typestack_id = stack_state_to_substacks.at(u64(state)).second;
			auto idx          = getIdxOf(state, name);
			return typestack.at(typestack_id, idx);
		}

		bool eqTypes(StackStateID state_1, StackStateID state_2) {
			auto typestack_id_1 = stack_state_to_substacks.at(u64(state_1)).second;
			auto typestack_id_2 = stack_state_to_substacks.at(u64(state_2)).second;

			return typestack.eq(typestack_id_1, typestack_id_2);
		}

		LocalStackDb();

	private:
		using NameMap = std::map<Lifetime, NameStackID>;
		base::HashMap<base::StrID, NameMap>                  name_to_namestack_id{};
		std::vector<NameStackEntry>                          namestack_entries{};
		std::vector<std::pair<NameStackID, TypeStackID>>     stack_state_to_substacks{};
		persistent::DummyVector<CRef<valid_type::ValidType>> typestack{};

		LocalStackDb(
			const decltype(name_to_namestack_id)&     name_to_id,
			const decltype(namestack_entries)&        entries,
			const decltype(stack_state_to_substacks)& stack_state_to_name_states,
			const decltype(typestack)&                typestack

		):
			  name_to_namestack_id(name_to_id),
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

			for (const auto& [id, curr_entry]: enumerate(entries) | drop(1)) {
				auto prev_entry_id = curr_entry.prev;
				CORE_ASSERT(
					prev_entry_id < id, "Previous state must have been created before current"
				);
				number_of_children[prev_entry_id]++;
				const auto& prev_entry = entries[prev_entry_id];

				auto [curr_r, curr_l] = curr_entry.lifetime;
				CORE_ASSERT(
					curr_l < intervals.size() && curr_r < intervals.size(),
					"lifetime idx is limited by number of states"
				);
				CORE_ASSERT(curr_l < curr_r, "init idx must be smaller than deinit idx");
				CORE_ASSERT(
					!intervals.at(curr_l) && !intervals.at(curr_r), "All interval ends are unique"
				);
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


			for (const auto& [name, maps]: name_to_id) {
				counted_ids += maps.size();

				CORE_ASSERT(maps.size(), "Each name must have at least correlated namestack state");

				for (auto& [lifetime, id]: maps) {
					CORE_ASSERT(id < entries.size(), "id has to point to valid name-stack entry");
					CORE_ASSERT(
						number_of_children[id] != 0, "There are still slots available for children"
					);
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
		}
	};

	class LocalStackDbBuilder {
		using tp_size        = LocalStackDb::tp_size;
		using Lifetime       = LocalStackDb::Lifetime;
		using NameMap        = LocalStackDb::NameMap;
		using NameStackEntry = LocalStackDb::NameStackEntry;
		using NameStackID    = LocalStackDb::NameStackID;
		using TypeStackID    = LocalStackDb::TypeStackID;

		struct Child {
			base::StrID name;
			tp_size     byte_offset;
		};

		using ChildHash = decltype([](const Child& child) {
			return std::hash<base::StrID>{}(child.name)
			     + std::hash<valid_type::TypeSize>{}(child.byte_offset);
		});

		struct TreeNode {
			base::HashMap<Child, NameStackID> children;
			usize                             depth;
			NameStackID                       prev;
			tp_size                           byte_depth;

			NameStackID emplaceChild(const Child& child, NameStackID new_id) {
				if_opt_some(children.atMaybeCopy(child), node_id) return node_id;

				children.put(child, new_id);
				return new_id;
			}
		};

		struct PushOp {
			CRef<valid_type::ValidType> type;
		};

		struct PopOp {};

		struct ChangeOp {
			base::StrID                 var_name;
			CRef<valid_type::ValidType> type;
		};

		struct CompletedOp {
			TypeStackID type_stack_id;
		};

		struct TypeOp {
			NameStackID                                        name_stack_id;
			usize                                              idx_prev;
			std::variant<PushOp, PopOp, ChangeOp, CompletedOp> op;
		};

		std::vector<TreeNode> tree;

		std::vector<TypeOp> to_lazy_process{};

		[[nodiscard]]
		NameStackID getTreeNodeId(StackStateID state) const {
			return to_lazy_process.at(u64{ state }).name_stack_id;
		}

		[[nodiscard]]
		TypeStackID getTypeStackId(StackStateID state) const {
			variant_match(to_lazy_process.at(u64{ state }).op) {
				variant_case(CompletedOp, calculated) { return calculated.type_stack_id; }
				variant_default { CORE_UNREACHABLE(); }
			}
			CORE_UNREACHABLE();
		}

	public:
		StackStateID push(StackStateID state, base::StrID name, CRef<valid_type::ValidType> type) {
			auto node_id = getTreeNodeId(state);
			auto child   = Child{
				  .name        = name,
				  .byte_offset = tree[node_id].byte_depth + type->getSize(),
			};

			auto new_node_id = tree[node_id].emplaceChild(child, tree.size());

			if (new_node_id == tree.size()) {
				tree.emplace_back(
					TreeNode{
						.children   = {},
						.depth      = tree[node_id].depth + 1,
						.prev       = node_id,
						.byte_depth = tree[node_id].byte_depth + type->getSize(),
					}
				);
			}

			to_lazy_process.emplace_back(
				TypeOp{
					.name_stack_id = new_node_id,
					.idx_prev      = u64{ state },
					.op            = PushOp{ .type = type },
				}
			);

			return StackStateID{ to_lazy_process.size() };
		}

		StackStateID pop(StackStateID state) {
			auto node_id = getTreeNodeId(state);
			CORE_ASSERT(node_id != 0, "We are not trying to remove the root");
			auto new_node_id = tree[node_id].prev;

			to_lazy_process.emplace_back(
				TypeOp{
					.name_stack_id = new_node_id,
					.idx_prev      = u64{ state },
					.op            = PopOp{},
				}
			);

			return StackStateID{ to_lazy_process.size() };
		}

		StackStateID change(StackStateID state, base::StrID name, CRef<valid_type::ValidType> type) {
			auto node_id = getTreeNodeId(state);

			to_lazy_process.emplace_back(
				TypeOp{
					.name_stack_id = node_id,
					.idx_prev = u64{ state },
					.op       = ChangeOp{
						.var_name = name,
						.type     = type,
					}, 
				}
			);

			return StackStateID{ to_lazy_process.size() };
		}

		LocalStackDb finalize() {
			usize order = 0;

			base::HashMap<base::StrID, NameMap>              name_to_namestack_id{};
			std::vector<NameStackEntry>                      namestack_entries(tree.size());
			std::vector<std::pair<NameStackID, TypeStackID>> stack_state_to_substacks{
				to_lazy_process.size()
			};
			persistent::DummyVector<CRef<valid_type::ValidType>> typestack{};


			auto dfs = [&](auto&&                                   self,
			               NameStackID                              node_id,
			               NameStackID                              prev,
			               base::HashMap<base::StrID, NameStackID>& names) -> void {
				CORE_ASSERT(node_id < tree.size(), "this should be true");
				Lifetime node_lifetime{};
				node_lifetime.init_idx = order;
				order++;

				for (auto& [child, next_node]: tree[node_id].children) {
					if (names.contains(child.name))
						throw std::invalid_argument("found a duplicate var-name in the same stack");

					names.emplace(child.name, next_node);

					self(self, next_node, node_id, names);
					names.erase(child.name);
				}

				node_lifetime.deinit_idx = order;
				order++;

				namestack_entries.at(node_id) = NameStackEntry{
					.lifetime       = node_lifetime,
					.prev           = prev,
					.size_in_bytes  = tree[node_id].byte_depth,
					.size_in_blocks = tree[node_id].depth,
				};

				for (auto& [child, next_node]: tree[node_id].children) {
					name_to_namestack_id.put(child.name, {});
					name_to_namestack_id.at(child.name).emplace(node_lifetime, node_id);
				}
			};

			base::HashMap<base::StrID, NameStackID> names = {};

			dfs(dfs, 0, 0, names);


			CORE_ASSERT(
				to_lazy_process.size(), "we need to have at least empty (already calculated) root"
			);

			using namespace std::views;
			for (auto [idx, oper]: enumerate(to_lazy_process) | drop(1)) {
				auto prev = oper.idx_prev;

				TypeStackID prev_typestack_id{};
				TypeStackID new_typestack_id{};

				variant_match(to_lazy_process.at(prev).op) {
					variant_case(CompletedOp, calculated) {
						prev_typestack_id = calculated.type_stack_id;
					}
					variant_default { CORE_UNREACHABLE(); }
				}

				variant_match(oper.op) {
					variant_case_novalue(CompletedOp) { CORE_UNREACHABLE(); }
					variant_case(PopOp, pop_oper) {
						new_typestack_id = typestack.pop(prev_typestack_id);
					}
					variant_case(PushOp, push_oper) {
						new_typestack_id = typestack.push(prev_typestack_id, push_oper.type);
					}
					variant_case(ChangeOp, change_oper) {
						auto curr_lifetime = namestack_entries.at(oper.name_stack_id).lifetime;

						auto& occurences = name_to_namestack_id.at(change_oper.var_name);

						auto it = occurences.lower_bound(curr_lifetime);

						if (it == occurences.end())
							throw std::invalid_argument("No such name at given stack");

						if (auto [init, deinit] = it->first;
						    init > curr_lifetime.deinit_idx || deinit < curr_lifetime.init_idx) {
							throw std::invalid_argument(
								"Variable with given name is not present at the stack instance"
							);
						}

						auto val_stack_id = it->second;

						usize name_to_idx_mangling
							= namestack_entries.at(val_stack_id).size_in_blocks;
						new_typestack_id = typestack.change(
							prev_typestack_id, name_to_idx_mangling, change_oper.type
						);
					}
				}

				stack_state_to_substacks.at(usize(idx))
					= std::make_pair(oper.name_stack_id, new_typestack_id);
			}

			return LocalStackDb(
				name_to_namestack_id, namestack_entries, stack_state_to_substacks, typestack
			);
		}
	};
}
