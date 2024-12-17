#include "type_metadata.hpp"
#include <base/exceptions.hpp>

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");

		TypeID      id        = type.getID();
		base::StrID type_name = type.getName();
		CORE_ASSERT(!types.contains(id), "Duplicated type");
		type_ids.put(type_name, id);
		types.put(id, std::move(type));
		return &types[id];
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

		for (auto& type: types) type.second->finalize();
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeByName(base::StrID name) const {
		auto opt_id = type_ids.atMaybe(name);
		if (opt_id) return &types[*opt_id];
		return {};
	}

	TypeCRef TypeMetadata::getType(TypeID id) const {
		CORE_ASSERT(types.contains(id), "Unknown type");
		return &types[id];
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeSafe(TypeID id) const {
		if (types.contains(id)) return &types[id];
		return {};
	}
}
