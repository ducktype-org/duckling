#pragma once

#include <config/cli_args.hpp>
#include <string>

namespace tester {
	struct TestConfig {
		std::string test_files_path;
	};

	TestConfig testConfigFromArgs(config::CLIArgs args, std::string_view path_to_test_from_dev);
}  // namespace tester
