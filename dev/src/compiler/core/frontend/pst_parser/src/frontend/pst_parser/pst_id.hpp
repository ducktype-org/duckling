// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <functional>

namespace pst {
	/**
	 * @brief Unique ID for each PST element.
	 * It can be used as session-unstable PST element hash.
	 */
	STRONG_TYPEDEF_ID(PstID);
}

ID_STD_HASH(::pst::PstID);
