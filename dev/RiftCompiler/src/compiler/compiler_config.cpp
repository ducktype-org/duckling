#include "compiler_config.hpp"
#include "clap/clap.hpp"
#include "clap/param_builder.hpp"
#include "clap/exceptions.hpp"
#include "clap/help_message_generator.hpp"

namespace compiler {
	clap::Clap compilerOptions() {
		return clap::Clap()
		    .addHelpFlag()
		    .add(clap::ParamBuilder::ofValue(clap::StringParser::make("file"))
		             .addShortName('o')
		             .addLongName("output")
		             .addShortDesc("Set output file")
		             .build())
		    .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
		             .required()
		             .addLongName("intTest")
		             .addShortDesc("Test number")
		             .build());
	}

	CompilerConfig fromArgs(clap::CLIArgs args) {
		auto parsed_args = compilerOptions().parse(args);

		CompilerConfig out;

		auto output = parsed_args.getValue<std::string>("output");
		if_opt_some(output, value) {
			out.was_output = true;
			out.output     = value;
		}

		auto intTest                         = parsed_args.getValue<std::string>("intTest");
		if_opt_some(intTest, val) out.output = val;

		for (usize i = 0; i < parsed_args.getExtraParameterCount(); i++)
			out.file_names.emplace_back(parsed_args.getExtra<std::string>(i).value());

		return out;
	}

	printer::Message generateHelpMessage(const clap::exceptions::HelpException& e) {
		auto             options = compilerOptions();
		printer::Message out({ "" });
		out.add(clap::HelpMessageGenerator::generate(options, e.parsing_result));
		return out;
	}

}
