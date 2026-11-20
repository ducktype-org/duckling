// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

namespace vm::code {
	/**
	 * @brief This is a common base for all element of high-level
	 * VM code representation.
	 *
	 * @note In the future, there will be more fields including
	 * e.g. link to a high-level language element that this element
	 * corresponds to.
	 */
	struct ElementBase {
		base::Optional<dia::SourcePosition> bytecode_pos = {};
	};
}
