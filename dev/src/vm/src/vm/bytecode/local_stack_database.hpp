#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace vm::persistent {

	template<typename T>
	class DummyVector {
		std::vector<std::pair<base::Optional<usize>, std::vector<T>>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		[[nodiscard]]
		usize pop(usize state, usize no_of_values_to_pop = 1) {
			for (; no_of_values_to_pop > 0; no_of_values_to_pop--) {
				auto& curr_state = validateState(state);
				if_opt_none(curr_state.first) break;
				state = *curr_state.first;
			}

			if (no_of_values_to_pop == 0) return state;

			if (state == EMPTY) throw std::invalid_argument("Trying to pop from an empty state");

			auto copy = validateState(state).second;

			for (; no_of_values_to_pop > 0; no_of_values_to_pop--) copy.pop_back();

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

		[[nodiscard]]
		usize size(usize state) const {
			return validateState(state).second.size();
		}

		DummyVector() { copies.emplace_back(base::Optional<usize>{}, std::vector<T>{}); }
	};

	template<typename Key, typename Val, typename Hasher = std::hash<Key>>
	class DummyHashMap {
		std::vector<base::HashMap<Key, Val, Hasher>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		[[nodiscard]]
		usize erase(usize state, const std::vector<Key>& removed_keys) {
			auto copy = validateState(state);

			for (auto removed_key: removed_keys) {
				if (!copy.contains(removed_key))
					throw std::invalid_argument("Trying to remove a non present key");

				copy.erase(removed_key);
			}

			copies.emplace_back(copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		usize insert(usize state, const Key& k, const Val& v) {
			auto copy = validateState(state);

			if (copy.contains(k)) throw std::invalid_argument("overriding a present value");
			copy.put(k, v);

			copies.emplace_back(copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		const Val& at(usize state, const Key& k) const {
			return validateState(state).at(k);
		}

		[[nodiscard]]
		bool contains(usize state, const Key& k) const {
			return validateState(state).contains(k);
		}

		[[nodiscard]]
		usize size(usize state) const {
			return validateState(state).size();
		}

		DummyHashMap() { copies.emplace_back(base::HashMap<Key, Val, Hasher>{}); }
	};
}

namespace vm::code {

	STRONG_TYPEDEF_INT(StackStateID, u64);

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
			tp_size     size_of_last{};
			base::StrID name_of_last = base::StrID{ "" };
		};

		base::Optional<NameStackEntry> getNameEntryByName(StackStateID state, base::StrID name) const;

		base::Optional<NameStackEntry> getNameEntryByIdx(StackStateID state, usize idx) const;

		[[nodiscard]]
		auto validateState(StackStateID state) const {
			CORE_ASSERT(u64{ state } < stack_state_to_substacks.size(), "State must be valid");
			return stack_state_to_substacks.at(u64{ state });
		}


	public:
		static constexpr StackStateID EMPTY            = StackStateID{ 0 };
		static constexpr NameStackID  EMPTY_NAME_STACK = 0;
		static constexpr TypeStackID EMPTY_TYPE_STACK = persistent::DummyVector<base::StrID>::EMPTY;

		base::Optional<tp_size> getByteOffset(StackStateID state, base::StrID name) const;

		bool contains(StackStateID state, base::StrID name) const;

		base::Optional<usize> getIdx(StackStateID state, base::StrID name) const;

		base::Optional<base::StrID> getTypeName(StackStateID state, base::StrID name) const;

		base::Optional<base::StrID> getTypeName(StackStateID state, usize idx) const;

		base::Optional<base::StrID> getName(StackStateID state, usize idx) const;

		usize size(StackStateID state) const;

		tp_size byteSize(StackStateID state) const;

		bool eqTypes(StackStateID state_1, StackStateID state_2) const;

		LocalStackDb();

	private:
		using NameMap = std::map<Lifetime, NameStackID>;
		base::HashMap<base::StrID, NameMap>              name_to_namestack{};
		std::vector<NameStackEntry>                      namestack_entries{};
		std::vector<std::pair<NameStackID, TypeStackID>> stack_state_to_substacks{};
		persistent::DummyVector<base::StrID>             typestack{};
		std::vector<NameMap>                             nodes_at_depth;

		LocalStackDb(
			const decltype(name_to_namestack)&        name_to_id,
			const decltype(namestack_entries)&        entries,
			const decltype(stack_state_to_substacks)& stack_state_to_name_states,
			const decltype(typestack)&                typestack

		);
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

			bool operator==(const Child&) const = default;
		};

		using ChildHash = decltype([](const Child& child) {
			return std::hash<base::StrID>{}(child.name)
			     + std::hash<valid_type::TypeSize>{}(child.byte_offset);
		});

		struct TreeNode {
			base::HashMap<Child, NameStackID, ChildHash> children{};
			usize                                        name_map_id   = 0;
			usize                                        name_stack_id = 0;
			usize                                        size          = 0;
			NameStackID                                  prev_node     = 0;
			tp_size byte_depth = tp_size{ Bytes{ 0 }, Bytes{ 0 } };

			NameStackID emplaceChild(const Child& child, NameStackID new_id);
		};

		[[nodiscard]]
		auto validateState(StackStateID state) const {
			return states.at(u64{ state });
		}

		const valid_type::ValidTypeMap& types_ctx;
		std::vector<TreeNode>           tree = { TreeNode{} };

		persistent::DummyHashMap<base::StrID, usize>     name_to_idx;
		persistent::DummyVector<base::StrID>             typenames;
		persistent::DummyVector<base::StrID>             var_names;
		std::vector<std::pair<NameStackID, TypeStackID>> states
			= { std::make_pair(LocalStackDb::EMPTY_NAME_STACK, LocalStackDb::EMPTY_TYPE_STACK) };

	public:
		LocalStackDbBuilder(const valid_type::ValidTypeMap& types_ctx): types_ctx(types_ctx) {}

		static constexpr StackStateID EMPTY = StackStateID{ 0 };

		StackStateID push(StackStateID state, base::StrID name, base::StrID type);

		StackStateID pop(StackStateID state, usize amount = 1);

		StackStateID change(StackStateID state, base::StrID name, base::StrID type);

		[[nodiscard]]
		base::Optional<base::StrID> typeOf(StackStateID state, base::StrID name) const;

		[[nodiscard]]
		base::Optional<base::StrID> typeOf(StackStateID state, usize idx) const;

		[[nodiscard]]
		usize size(StackStateID state) const;

		[[nodiscard]]
		bool contains(StackStateID state, base::StrID name) const;

		[[nodiscard]]
		bool eqTypes(StackStateID state_1, StackStateID state_2) const;

		[[nodiscard]]
		base::Optional<base::StrID> getName(StackStateID state, usize idx) const;

		LocalStackDb finalize();
	};
}
