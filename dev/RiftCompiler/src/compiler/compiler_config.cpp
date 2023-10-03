#include "compiler_config.hpp"

#include <config/config.hpp>

namespace compiler {
	config::ConfigOptions compilerOptions() {
		config::ConfigOptions out;
		out.addOption("help", "h", "Displays help");
		out.addOption(
			"output",
			"o",
			config::ParamType::Always,
			base::make_unique<config::StringParser>(),
			"Set output file"
		);
		out.addOption(
			"intTest",
			config::ParamType::Always,
			base::make_unique<config::IntParser>(),
			"Test option"
		);
		return out;
	}

	CompilerConfig fromArgs(config::CLIArgs args) {
		auto parsed_args = config::parse(compilerOptions(), args);

		CompilerConfig out;

		if (parsed_args.wasOption("help")) out.was_help = true;

		if (parsed_args.wasOption("output")) {
			out.was_output = true;
			out.output     = parsed_args.getValue<std::string>("output");
		}

		if (parsed_args.wasOption("intTest")) out.output = parsed_args.getValue<i32>("intTest");

		for (auto file: parsed_args.getNonOptionValues())
			out.file_names.emplace_back(file.stdString());

		return out;
	}

	printer::Message generateHelpMessage() {
		auto             options = compilerOptions();
		printer::Message out({ "" });
		out.add("Usage: rift [options] files\n");
		out.add("Options:\n");
		options.generateOptionDesc(out);
		return out;
	}

}
