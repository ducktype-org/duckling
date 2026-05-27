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
#include <vm/utils/persistent/dummy/hashmap.hpp>
#include <vm/utils/persistent/dummy/vector.hpp>

#include <utility>

namespace vm::code {

	STRONG_TYPEDEF_ID_DIRECT_CREATION(StackStateID, u64);

	class LocalStackDbBuilder;

	/**
	 * @brief A class representing an immutable database for the stack. Each state of the stack has
	 * it is assigned ID, which has to be passed as a key to query the database
	 */
	class LocalStackDb {
		friend LocalStackDbBuilder;

		using NameStackID = u64;
		using TypeStackID = u64;

		struct Lifetime {
			usize deinit_idx                         = 0;
			usize init_idx                           = 0;
			auto  operator<=>(const Lifetime&) const = default;
		};

		struct NameStackEntry {
			Lifetime             lifetime{};
			NameStackID          prev = 0;
			valid_type::TypeSize size_in_bytes{};
			usize                size_in_blocks{};
			valid_type::TypeSize size_of_last{};
			base::StrID          name_of_last = base::StrID{ "" };
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

		base::Optional<valid_type::TypeSize> getByteOffset(StackStateID state, base::StrID name)
			const;

		bool contains(StackStateID state, base::StrID name) const;

		base::Optional<usize> getIdx(StackStateID state, base::StrID name) const;

		base::Optional<base::StrID> getTypeName(StackStateID state, base::StrID name) const;

		base::Optional<base::StrID> getTypeName(StackStateID state, usize idx) const;

		base::Optional<base::StrID> getName(StackStateID state, usize idx) const;

		usize size(StackStateID state) const;

		valid_type::TypeSize byteSize(StackStateID state) const;

		[[nodiscard]]
		bool eqTypes(StackStateID state_1, StackStateID state_2) const;

		[[nodiscard]]
		bool eqNames(StackStateID state_1, StackStateID state_2) const;

		LocalStackDb();

	private:
		using NameMap = std::map<Lifetime, NameStackID>;
		base::HashMap<base::StrID, NameMap>              name_to_namestack{};
		std::vector<NameStackEntry>                      namestack_entries{};
		std::vector<std::pair<NameStackID, TypeStackID>> stack_state_to_substacks{};
		persistent::DummyVector<base::StrID>             typestack{};
		std::vector<NameMap>                             nodes_at_depth;

		LocalStackDb(
			const decltype(name_to_namestack)& name_to_id,
			const decltype(namestack_entries)& entries,
			decltype(stack_state_to_substacks) stack_state_to_name_states,
			decltype(typestack)                typestack

		);
	};
}
