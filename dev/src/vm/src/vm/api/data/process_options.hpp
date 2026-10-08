// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace vm::api {
	enum class ProcessMode { Safe, Fast };

	struct ProcessConfig final {
		ProcessMode mode                      = ProcessMode::Safe;
		bool        enable_deadlock_detection = false;
		/// Only meaningful for the Safe mode in JIT builds; ignored otherwise.
		bool enable_jit = true;
	};
}
