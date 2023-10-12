
#include "tester_config.hpp"
#include <config/config.hpp>

namespace tester {

	TestConfig testConfigFromArgs(config::CLIArgs args, std::string_view path_to_test_from_dev) {
		auto parsed_args = config::parse(config::ConfigOptions(), args);

		if (parsed_args.getNonOptionValues().size() > 1)
			throw base::Panic("Config", "Test configuration expects no more then one path name");

		TestConfig out;
		if (parsed_args.getNonOptionValues().size() == 1)
			out.test_files_path = parsed_args.getNonOptionValues()[0].stdString();
		else
			out.test_files_path = "./";

		out.test_files_path += path_to_test_from_dev;

		return out;
	}
}
