#pragma once

#include <string>
#include "base/ints.hpp"
#include "clap/clap.hpp"

namespace tester {
	struct TestConfig {
		std::string test_files_path;
	};

	TestConfig testConfigFromArgs(clap::CLIArgs args, std::string_view path_to_test_from_dev);
}
