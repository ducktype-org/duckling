#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/local_stack_database.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/persistent/dummy/hashmap.hpp>
#include <vm/utils/persistent/dummy/vector.hpp>

namespace vm::code {
	/**
	 * @brief this structure records the pops and pushes to the stack, and later constructs an
	 * immutable local database which contains all the information
	 * @note It allows for the same operations as local stack databes, though a bit slower
	 */
	class LocalStackDbBuilder {
		using Lifetime       = LocalStackDb::Lifetime;
		using NameMap        = LocalStackDb::NameMap;
		using NameStackEntry = LocalStackDb::NameStackEntry;
		using NameStackID    = LocalStackDb::NameStackID;
		using TypeStackID    = LocalStackDb::TypeStackID;

		struct Child {
			base::StrID          name;
			valid_type::TypeSize byte_offset;

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
			valid_type::TypeSize byte_depth = valid_type::TypeSize{ Bytes{ 0 }, Bytes{ 0 } };

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
		bool eqNames(StackStateID state_1, StackStateID state_2) const;

		[[nodiscard]]
		base::Optional<base::StrID> getName(StackStateID state, usize idx) const;

		LocalStackDb finalize();
	};
}
