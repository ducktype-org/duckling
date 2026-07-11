#pragma once

#include "../definitions.hpp"
#include "../inheritance_metadata.hpp"

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace vm::kind {

	struct FieldDesc final {
		Offset       offset;
		ShadowOffset shadow_offset;
		TypeRef      type;
	};

	struct Data final {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		base::HashMap<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>              fields;

		base::Optional<InheritanceMetadata> inheritance_metadata;

		/**
		 * @brief Shadow entry index of every byte of the type, indexed by byte offset. Padding
		 * bytes belong to no field and hold `NO_SHADOW_ENTRY`. Filled in by `Type::finalize`.
		 */
		std::vector<ShadowOffset> byte_to_shadow;
	};
}
