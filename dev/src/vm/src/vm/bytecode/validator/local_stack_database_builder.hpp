#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/local_stack_database.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/persistent/hashmap.hpp>
#include <vm/utils/persistent/vector.hpp>

namespace vm::code {
	/**
	 * @brief this structure records the pops and pushes to the stack, and later constructs an
	 * immutable local database which contains all the information
	 * @note It allows for the same operations as local stack databes, though a bit slower since
	 * they have to be performed online
	 */
	class LocalStackDbBuilder {
		using Lifetime       = LocalStackDb::Lifetime;
		using NameMap        = LocalStackDb::NameMap;
		using NameStackEntry = LocalStackDb::NameStackEntry;
		using NameStackID    = LocalStackDb::NameStackID;
		using TypeStackID    = LocalStackDb::TypeStackID;

		struct Child final {
			base::StrID          name;
			valid_type::TypeSize byte_offset;

			bool operator==(const Child&) const = default;
		};

		struct ChildHash final {
			constexpr usize operator()(const Child& child) const {
				return std::hash<base::StrID>{}(child.name)
				     + child.byte_offset.assumePointerSize(Bytes(16)).asInt()
				     + child.byte_offset.assumePointerSize(Bytes(8)).asInt();
			}
		};

		struct TreeNode final {
			base::HashMap<Child, NameStackID, ChildHash> children{};
			persistent::HashMapStateID name_map_id = persistent::HashMap<base::StrID, usize>::EMPTY;
			persistent::VectorStateID  name_stack_id = persistent::Vector<base::StrID>::EMPTY;
			usize                      size          = 0;
			NameStackID                prev_node     = 0;
			valid_type::TypeSize       byte_depth = valid_type::TypeSize{ Bytes{ 0 }, Bytes{ 0 } };

			NameStackID emplaceChild(const Child& child, NameStackID new_id);
		};

		[[nodiscard]]
		auto validateState(StackStateID state) const {
			return states.at(u64{ state });
		}

		const valid_type::ValidTypeMap& types_ctx;
		std::vector<TreeNode>           tree = { TreeNode{} };

		persistent::HashMap<base::StrID, usize>          name_to_idx;
		persistent::Vector<base::StrID>                  typenames;
		persistent::Vector<base::StrID>                  var_names;
		std::vector<std::pair<NameStackID, TypeStackID>> states
			= { std::make_pair(LocalStackDb::EMPTY_NAME_STACK, LocalStackDb::EMPTY_TYPE_STACK) };

	public:
		LocalStackDbBuilder(const valid_type::ValidTypeMap& types_ctx): types_ctx(types_ctx) {}

		static constexpr StackStateID EMPTY = StackStateID{ 0 };

		/**
		 * @brief operation which pushes new variable of given name and stack to the givens state of
		 * the stack
		 * @returns id of modified state
		 */
		[[nodiscard]]
		StackStateID push(StackStateID state, base::StrID name, base::StrID type);

		/**
		 * @brief operation which pops a caretain amount of last variables from the stack
		 * @note popped amount must be smaller than size of the stack at given state
		 * @returns id of modified state
		 */
		[[nodiscard]]
		StackStateID pop(StackStateID state, usize amount = 1);

		/**
		 * @brief operation which changes the type of variable with given name
		 * @note there must be a variable with given name on the stack at given state
		 * @returns id of modified state
		 */
		[[nodiscard]]
		StackStateID change(StackStateID state, base::StrID name, base::StrID type);

		/**
		 * @returns name of the type of the variable with particular name at given state of the stack
		 * @note this information takes into account primitive-casting operations
		 * @note returns nullopt, when stack at current state doesn't contain variable with such name
		 */
		[[nodiscard]]
		base::Optional<base::StrID> getTypeName(StackStateID state, base::StrID name) const;

		/**
		 * @returns name of the type of the variable with particular index at given state of the stack
		 * @note this information takes into account primitive-casting operations
		 * @note returns nullopt, when stack at current state doesn't contain variable with such index
		 */
		[[nodiscard]]
		base::Optional<base::StrID> getTypeName(StackStateID state, usize idx) const;

		/**
		 * @returns number of variables on the stack (size of the stack in blocks) at given state
		 */
		[[nodiscard]]
		usize size(StackStateID state) const;

		/**
		 * @brief checks whether stack at given state contains a value of given name
		 */
		[[nodiscard]]
		bool contains(StackStateID state, base::StrID name) const;

		/**
		 * @brief compares two states of the stack in terms of types of variables
		 * @returns true if two instances are the same in terms of types of the variables and their
order order	 * @note this information takes into account primitive-casting operations
		 */
		[[nodiscard]]
		bool eqTypes(StackStateID state_1, StackStateID state_2) const;

		/**
		 * @brief compares two states of the stack in terms of names of variables
		 * @returns true if two instances are the same in terms of names of the variables and their
		 * order
		 */
		[[nodiscard]]
		bool eqNames(StackStateID state_1, StackStateID state_2) const;

		/**
		 * @returns name of the variable with particular index at given state of the stack
		 * @note returns nullopt, when stack at current state doesn't contain variable with such index
		 */
		[[nodiscard]]
		base::Optional<base::StrID> getName(StackStateID state, usize idx) const;

		/**
		 * @brief function to finish building th local stack database
		 * @returns a database which will anwer all the questions offline
		 * @note this function should be called exactly once, at the end of lifetime for the database
		 */
		LocalStackDb finalize();
	};
}
