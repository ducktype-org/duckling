// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "tester_config.hpp"

namespace tester {

	TestConfig getTestConfig(std::string_view path_to_test_from_dev) {
		TestConfig out;
		out.test_files_path = std::string(CMAKE_SOURCE_DIR) + std::string(path_to_test_from_dev);

		return out;
	}
}
