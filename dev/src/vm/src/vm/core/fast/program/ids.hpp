// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(FunctionID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(GlobalDataID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);

	/**
	 * @brief Enum of all instruction IDs.
	 */
	enum class InstrID : u64 {
#define HANDLE_INSTR(NAME) NAME,
#include "instructions/instruction_definitions.def.hpp"
#undef HANDLE_INSTR
	};
}
