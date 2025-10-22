#include "type_metadata.hpp"

#include <base/except/exceptions.hpp>

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(
			state == TypeMetadataState::AddingTypes,
			"Tried to add types to type metadata when it was not in the adding types state"
		);

		base::StrID name = type.name;
		TypeID      id   = types.insert(std::move(type), name);
		TypeRef     ref  = types.at(id);
		ref->id          = id;

		return ref;
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(
			state == TypeMetadataState::AddingTypes,
			"Tried to finalize type metadata when it was not in the AddingTypes state"
		);
		state = TypeMetadataState::Finalized;

		for (auto& tp: types) tp.finalize();
	}

	void TypeMetadata::unfinalize() {
		if (state == TypeMetadataState::Finalized) state = TypeMetadataState::AddingTypes;
	}

	TypeCRef TypeMetadata::at(TypeID id) const { return types.at(id); }

	base::Optional<TypeCRef> TypeMetadata::atMaybe(TypeID id) const { return types.atMaybe(id); }

	base::Optional<TypeRef> TypeMetadata::atMaybe(TypeID id) { return types.atMaybe(id); }

	base::Optional<TypeCRef> TypeMetadata::atMaybe(base::StrID name) const {
		return types.atMaybe(name);
	}

	base::Optional<TypeRef> TypeMetadata::atMaybe(base::StrID name) { return types.atMaybe(name); }

	TypeRef TypeMetadata::at(TypeID id) { return types.at(id); }

	TypeRef TypeMetadata::at(base::StrID name) { return types.at(name); }

	TypeCRef TypeMetadata::at(base::StrID name) const { return types.at(name); }

}
