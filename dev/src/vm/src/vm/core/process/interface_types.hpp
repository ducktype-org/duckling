// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <vm/core/vmvalue/ivmvalue.hpp>

namespace vm {
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<CRef<vm::IVMValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;
}
