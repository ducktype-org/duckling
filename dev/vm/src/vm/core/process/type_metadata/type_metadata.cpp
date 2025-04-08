#include "type_metadata.hpp"

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");


		types.emplaceBack(std::move(type));
		auto id = TypeID::next();

		CORE_ASSERT(u64(id) == types.size() - 1, "bad TypeID");

		types.last()->id = id;
		types_ids.push_back(id);

		names_to_type.put(types.last()->getName(), id);

		return types.last();
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

		for (auto id: types_ids) types[u64(id)]->finalize();
	}

	TypeCRef TypeMetadata::getType(TypeID id) const {
		return types[u64(id)];
	}

	base::Optional<TypeCRef> TypeMetadata::getTypeByName(base::StrID name) const {
		if (names_to_type.contains(name))
			return getType(names_to_type[name]);
		else
			return {};
	}

}
