// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/stable_position.hpp>

namespace compiler::mir {
	struct InstructionMetadata {
		base::Optional<dia::StablePosition> position;
	};
}
