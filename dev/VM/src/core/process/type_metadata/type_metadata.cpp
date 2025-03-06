#include "type_metadata.hpp"

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");


		auto id      = types.emplaceBack(std::move(type));
		types[id].id = id;
		types_ids.push_back(id);

		names_to_type.put(types[id].getName(), id);

		return types.getRef(id).value();
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

		for (auto id: types_ids) types[id].finalize();
	}

	TypeCRef TypeMetadata::getType(TypeID id) const {
		return types.getCRef(id).expect("Bad TypeID in getType");
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeSafe(TypeID id) const {
		return types.getCRef(id);
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeByName(base::StrID name) const {
		if (names_to_type.contains(name))
			return getType(names_to_type[name]);
		else
			return {};
	}

}
