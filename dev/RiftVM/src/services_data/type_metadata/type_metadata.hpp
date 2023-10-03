#pragma once

#include <base/stable_container.hpp>

#include "type.hpp"

namespace vm {
	template<class... DynamicData>
	class DataManagerDef;

	class TypeMetadata {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		base::StableList<TypeId, Type> types;
		std::vector<TypeId>            types_ids;
		base::Map<base::StrId, TypeId> names_to_type;

		TypeMetadataState state;

		TypeMetadata(): state(TypeMetadataState::AddingTypes){};

	public:
		TypeRef addType(Type&& type);

		void finalize();

		TypeCRef         getType(TypeId id) const;
		option<TypeCRef> getTypeSafe(TypeId id) const;

		// @TODO: This function is currently used by parser, but
		// should be deleted in the future
		option<TypeCRef> getTypeByName(base::StrId name) const;

		template<class... DynamicData>
		friend class DataManagerDef;
	};
}  // namespace vm
