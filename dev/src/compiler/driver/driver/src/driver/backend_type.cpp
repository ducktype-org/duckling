// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "backend_type.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::driver {

	std::string backendTypeToStr(BackendType type) {
		switch (type) {
		case BackendType::LLVM:
			return "llvm";
		case BackendType::DVM:
			return "dvm";
		default:
			CORE_UNREACHABLE();
		}
	}
}
