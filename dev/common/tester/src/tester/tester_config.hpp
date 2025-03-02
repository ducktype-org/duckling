#pragma once

#include <string>

namespace tester {
	struct TestConfig final {
		std::string test_files_path;
	};

	TestConfig testConfigFromArgs(std::string_view path_to_test_from_dev);
}
