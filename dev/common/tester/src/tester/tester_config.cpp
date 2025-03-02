
#include "tester_config.hpp"

namespace tester {

	TestConfig testConfigFromArgs(clap::CLIArgs args, std::string_view path_to_test_from_dev) {
		if (args.argc != 1)
			CORE_PANIC("Tester expects no arguments");

		TestConfig out;
		out.test_files_path = std::string(CMAKE_SOURCE_DIR) + std::string(path_to_test_from_dev);

		return out;
	}
}
