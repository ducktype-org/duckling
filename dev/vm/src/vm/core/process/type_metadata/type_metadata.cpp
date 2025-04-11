#include "type_metadata.hpp"

#include <base/exceptions.hpp>
#include <base/variant.hpp>

namespace vm {
	TypeRef TypeMetadata::addType(Type&& type) {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");

<<<<<<< HEAD
		types.emplaceBack(std::move(type));
		auto id = TypeID::fromU64(types.lastIndex());

		types.last()->id = id;
		types_ids.push_back(id);

		names_to_type.put(types.last()->getName(), id);

		return types.last();
=======
		base::StrID name = type.name;
		TypeID      id   = types.insert(std::move(type), name);
		TypeRef     ref  = types.at(id);
		ref->id          = id;

		return ref;
>>>>>>> origin/main
	}

	void TypeMetadata::finalize() {
		CORE_ASSERT(state == TypeMetadataState::AddingTypes, "bad TypeMetadata state");
		state = TypeMetadataState::Finalized;

<<<<<<< HEAD
		for (auto id: types_ids) types[u64(id)]->finalize();
	}

	TypeCRef TypeMetadata::getType(TypeID id) const { return types[u64(id)]; }
=======
		for (auto& tp: types) tp.finalize();
	}

	TypeCRef TypeMetadata::at(TypeID id) const { return types.at(id); }

	base::Optional<TypeCRef> TypeMetadata::atMaybe(TypeID id) const { return types.atMaybe(id); }

	base::Optional<TypeRef> TypeMetadata::atMaybe(TypeID id) { return types.atMaybe(id); }

	base::Optional<TypeCRef> TypeMetadata::atMaybe(base::StrID name) const {
		return types.atMaybe(name);
	}

	base::Optional<TypeRef> TypeMetadata::atMaybe(base::StrID name) { return types.atMaybe(name); }
>>>>>>> origin/main

	TypeRef TypeMetadata::at(TypeID id) { return types.at(id); }

	TypeRef TypeMetadata::at(base::StrID name) { return types.at(name); }

	TypeCRef TypeMetadata::at(base::StrID name) const { return types.at(name); }

}
