#pragma once

#include <base/stable_container.hpp>
#include "type.hpp"

namespace vm {
	class VMProcess;

	/**
	 * @brief Holds metadata about all types in the VCPU.
	 *
	 * This class is used to store and access all types
	 * that are used in the VCPU.
	 */
	class TypeMetadata {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		base::StableVector<Type, TypeID> types;
		std::vector<TypeID>              types_ids;
		base::Map<base::StrID, TypeID>   names_to_type;

		TypeMetadataState state;

		TypeMetadata(): state(TypeMetadataState::AddingTypes) {};

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

		friend class VMProcess;
	};
}
