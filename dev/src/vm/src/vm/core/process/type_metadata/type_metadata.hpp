#pragma once

#include "type.hpp"

#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm {
	/**
	 * @brief Holds metadata about all types in the program.
	 * @note Copy/Move constructors are deleted, because inner types hold cross-references to
	 * themselves, so moving or copying them may invalidate their state.
	 */
	class TypeMetadata final {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		StableObjIdNameMap<Type, TypeID> types;

		TypeMetadataState state{ TypeMetadata::TypeMetadataState::AddingTypes };

	public:
		TypeMetadata() = default;

		// Deleting copy and move constructors/operator= because pointers inside types
		TypeMetadata(const TypeMetadata&)            = delete;
		TypeMetadata(TypeMetadata&&) noexcept        = delete;
		TypeMetadata& operator=(const TypeMetadata&) = delete;
		TypeMetadata& operator=(TypeMetadata&&)      = delete;

		/**
		 * @note This function shouldn't be used after finalize().
		 */
		TypeRef addType(Type&& type);

		/**
		 * @brief Finalize adding types.
		 * @note This function shouldn't be called twice.
		 */
		void finalize();

		/**
		 * @brief Unfinalize the type metadata for injecting new types.
		 * If type metadata was already in AddingTypes state then nothing is done.
		 */
		void unfinalize();

		[[nodiscard]]
		TypeCRef at(TypeID id) const;
		TypeRef  at(TypeID id);

		[[nodiscard]]
		TypeCRef at(base::StrID name) const;
		TypeRef  at(base::StrID name);

		[[nodiscard]]
		base::Optional<TypeCRef> atMaybe(TypeID id) const;
		base::Optional<TypeRef>  atMaybe(TypeID id);

		[[nodiscard]]
		base::Optional<TypeCRef> atMaybe(base::StrID name) const;
		base::Optional<TypeRef>  atMaybe(base::StrID name);

		auto begin() const { return types.begin(); }

		auto begin() { return types.begin(); }

		auto end() const { return types.end(); }

		auto end() { return types.end(); }

		usize size() { return types.size(); }
	};
}
