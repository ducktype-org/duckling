// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <string>

namespace tester {
	struct TestConfig final {
		std::string test_files_path;
	};

	TestConfig getTestConfig(std::string_view path_to_test_from_dev);
}
