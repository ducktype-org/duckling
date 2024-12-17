#pragma once

#include <base/stable_container.hpp>
#include "base/stable_hashmap.hpp"
#include "type.hpp"

namespace vm {
	template<class... DynamicData>
	class DataManagerDef;

	/**
	 * @brief Holds metadata about all types in the VCPU.
	 *
	 * This class is used to store and access all types
	 * that are used in the VCPU.
	 */
	class TypeMetadata {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		base::StableHashMap<TypeID, Type>  types;
		base::HashMap<base::StrID, TypeID> type_ids;

		TypeMetadataState state{ TypeMetadata::TypeMetadataState::AddingTypes };

		TypeMetadata() = default;

	public:
		TypeRef addType(Type&& type);

		/**
		 * @brief Finalize adding types.
		 */
		void finalize();

		[[nodiscard]]
		TypeCRef getType(TypeID id) const;

		[[nodiscard]]
		base::Optional<TypeCRef> getTypeSafe(TypeID id) const;

		// @TODO: This function is currently used by parser, but
		// should be deleted in the future
		[[nodiscard]]
		base::Optional<TypeCRef> getTypeByName(base::StrID name) const;

		template<class... DynamicData>
		friend class DataManagerDef;

		friend class Program;
	};
}
