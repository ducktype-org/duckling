#include "type_metadata.hpp"

#include <base/exceptions.hpp>

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");

		base::StrID name = type.name;
		TypeID      id   = types.insert(std::move(type), name);
		TypeRef     ref  = types.at(id);
		ref->id          = id;

		return ref;
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

		for (auto& tp: types) tp.finalize();
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
