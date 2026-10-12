// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_flags.hpp"

namespace logger {
	// Dev logs are disabled by default.
	constinit bool enable_dev_logs = false;

	// User logs are enabled by default.
	constinit bool enable_user_logs = true;
}
