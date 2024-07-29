
#include "tester_config.hpp"
#include <clap/clap.hpp>

namespace tester {

	TestConfig testConfigFromArgs(clap::CLIArgs args, std::string_view path_to_test_from_dev) {
		auto parsed_args = clap::Clap().parse(args);

		if (parsed_args.getExtraParameterCount() > 1)
			throw base::Panic("Config", "Test configuration expects no more than one path name");

		TestConfig out;
		if (parsed_args.getExtraParameterCount() == 1)
			out.test_files_path = *parsed_args.getExtra<std::string>(0);
		else
			out.test_files_path = "./";

		out.test_files_path += path_to_test_from_dev;

		return out;
	}
}
