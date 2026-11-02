// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../definitions.hpp"
#include "../inheritance_metadata.hpp"

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace vm::kind {

	struct FieldDesc final {
		Offset  offset;
		TypeRef type;
	};

	struct Data final {
		// @todo: change to strongly typed when it will be in utils
		using FieldID = u64;

		base::HashMap<base::StrID, FieldID> field_name_map;
		std::vector<FieldDesc>              fields;

		base::Optional<InheritanceMetadata> inheritance_metadata;
	};
}
