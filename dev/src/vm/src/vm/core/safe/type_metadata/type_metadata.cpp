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

	base::Optional<std::pair<u64, u64>> TypeMetadata::seekMethodParamRetCount(
		const base::StrID& method_name
	) const {
		// @TODO: #962 Optimize
		auto it = std::ranges::find_if(types, [&](const auto& type) {
			if_opt_some(type.getInheritanceMetadata(), inh_meta) {
				return (*inh_meta).available_methods.contains(method_name);
			}
			return false;
		});
		if (it != types.end()) {
			auto inh_meta = it->getInheritanceMetadata().value();
			auto param    = inh_meta->available_methods[method_name]->getParameterCount();
			auto ret      = inh_meta->available_methods[method_name]->getResultTypeCount();
			if (!param)
				return std::nullopt;  // Either both param and ret are present or both are not.
			return std::make_pair(*param, *ret);
		}
		CORE_UNREACHABLE();
	}
}
