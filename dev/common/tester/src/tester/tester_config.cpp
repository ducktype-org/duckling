
#include "tester_config.hpp"
#include <clap/clap.hpp>

namespace tester {

	TestConfig testConfigFromArgs(clap::CLIArgs args, std::string_view path_to_test_from_dev) {
		auto parsed_args = clap::Clap().parse(args);

		if (parsed_args.getExtraParameterCount() > 0)
			throw base::Panic("Config", "Test configuration expects no extra parameters");

		TestConfig out;
		out.test_files_path = std::string(CMAKE_SOURCE_DIR) + std::string(path_to_test_from_dev);

		return out;
	}
}
