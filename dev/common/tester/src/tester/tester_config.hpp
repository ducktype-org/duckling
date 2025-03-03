#pragma once

#include <string>
#include <clap/clap.hpp>  // @TODO #404 relax it so it only includes CLIArgs

namespace tester {
	struct TestConfig final {
		std::string test_files_path;
	};

	TestConfig testConfigFromArgs(clap::CLIArgs args, std::string_view path_to_test_from_dev);
}
