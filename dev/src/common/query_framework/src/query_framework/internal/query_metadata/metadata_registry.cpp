// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "metadata_registry.hpp"

namespace query::internal {
	MetadataRegistry& MetadataRegistry::instance() {
		static MetadataRegistry inst;
		return inst;
	}

	bool MetadataRegistry::registerType(TypeID type_id, BytesDeserializeFunc deserialize_func) {
		CORE_ASSERT(
			!registry.contains(type_id), "Metadata type already registered:", type_id.strView()
		);
		registry.put(type_id, RegisterData{ .deserializer = deserialize_func });
		return true;
	}

	bool MetadataRegistry::registerStrIDType(TypeID type_id, StrIDDeserializeFunc deserialize_func) {
		CORE_ASSERT(
			!registry.contains(type_id), "Metadata type already registered:", type_id.strView()
		);
		registry.put(type_id, RegisterData{ .deserializer = deserialize_func });
		return true;
	}

	DeserializerVariant MetadataRegistry::getDeserializer(TypeID type_id) const {
		auto data_opt = registry.atMaybe(type_id);
		CORE_ASSERT(
			data_opt.has_value(),
			"Unknown metadata type_id: ",
			type_id.strView(),
			" (not registered)"
		);
		return data_opt.value()->deserializer;
	}

	bool MetadataRegistry::isRegistered(TypeID type_id) const { return registry.contains(type_id); }

	bool MetadataRegistry::isStrIDType(TypeID type_id) const {
		auto data_opt = registry.atMaybe(type_id);
		CORE_ASSERT(
			data_opt.has_value(),
			"Unknown metadata type_id: ",
			type_id.strView(),
			" (not registered)"
		);
		return std::holds_alternative<StrIDDeserializeFunc>(data_opt.value()->deserializer);
	}
}
