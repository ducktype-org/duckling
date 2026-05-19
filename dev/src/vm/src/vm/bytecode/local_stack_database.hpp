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
#include <variant>

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

		DummyVector() { copies.emplace_back(base::Optional<usize>{}, std::vector<T>{}); }
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
		};

		NameStackEntry getNameEntry(StackStateID state, base::StrID name) const;

	public:
		static constexpr usize       EMPTY            = 0;
		static constexpr NameStackID EMPTY_NAME_STACK = 0;
		static constexpr TypeStackID EMPTY_TYPE_STACK = persistent::DummyVector<base::StrID>::EMPTY;

		tp_size getByteOffset(StackStateID state, base::StrID name) const;

		bool contains(StackStateID state, base::StrID name) const;

		usize getIdxOf(StackStateID state, base::StrID name) const;

		base::StrID getTypeName(StackStateID state, base::StrID name) const;

		base::StrID getTypeName(StackStateID state, usize idx) const;

		usize size(StackStateID state) const;

		bool eqTypes(StackStateID state_1, StackStateID state_2);

		LocalStackDb();

	private:
		using NameMap = std::map<Lifetime, NameStackID>;
		base::HashMap<base::StrID, NameMap>              name_to_namestack_id{};
		std::vector<NameStackEntry>                      namestack_entries{};
		std::vector<std::pair<NameStackID, TypeStackID>> stack_state_to_substacks{};
		persistent::DummyVector<base::StrID>             typestack{};

		LocalStackDb(
			const decltype(name_to_namestack_id)&     name_to_id,
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
			usize                                        depth = 0;
			NameStackID                                  prev  = 0;
			tp_size byte_depth                                 = tp_size{ Bytes{ 0 }, Bytes{ 0 } };

			NameStackID emplaceChild(const Child& child, NameStackID new_id);
		};

		struct PushOp {
			base::StrID type;
		};

		struct PopOp {
			usize amount;
		};

		struct ChangeOp {
			base::StrID var_name;
			base::StrID type;
		};

		struct CompletedOp {
			TypeStackID type_stack_id;
		};

		struct TypeOp {
			NameStackID                                        name_stack_id;
			usize                                              idx_prev;
			std::variant<PushOp, PopOp, ChangeOp, CompletedOp> op;
		};

		const valid_type::ValidTypeMap& types_ctx;
		std::vector<TreeNode>           tree = {};
		std::vector<TypeOp>   to_lazy_process = {
			TypeOp {
				.name_stack_id = LocalStackDb::EMPTY_NAME_STACK,
				.idx_prev = 0, 
				.op = CompletedOp {
					.type_stack_id = LocalStackDb::EMPTY_TYPE_STACK,
				},
			}
		};

		[[nodiscard]]
		NameStackID getTreeNodeId(StackStateID state) const;

		[[nodiscard]]
		TypeStackID getTypeStackId(StackStateID state) const;

	public:
		LocalStackDbBuilder(const valid_type::ValidTypeMap& types_ctx): types_ctx(types_ctx) {}

		static constexpr StackStateID EMPTY = StackStateID{0};

		StackStateID push(StackStateID state, base::StrID name, base::StrID type);

		StackStateID pop(StackStateID state, usize amount = 1);

		StackStateID change(StackStateID state, base::StrID name, base::StrID type);

		[[nodiscard]]
		usize size(StackStateID state) const;

		LocalStackDb finalize();
	};
}
