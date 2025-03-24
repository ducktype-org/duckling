#include "type_metadata.hpp"

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");

		auto id = TypeID(types.size());
		types.push_back(std::move(type));
		types.back().id = id; // This is not in the new structure
		types_ids.push_back(id);

		names_to_type.put(getType(id)->getName(), id);

		return &types.back();
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

		for (auto& tp: types) tp.finalize();
	}

	TypeCRef TypeMetadata::getType(TypeID id) const {
		CORE_ASSERT(usize(id) <= types.size(), "Invalid type id");
		return &types[usize(id)];
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeSafe(TypeID id) const {
		if (usize(id) > types.size()) return {};
		return &types[usize(id)];
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeByName(base::StrID name) const {
		if (names_to_type.contains(name))
			return getType(names_to_type[name]);
		else
			return {};
	}

}
