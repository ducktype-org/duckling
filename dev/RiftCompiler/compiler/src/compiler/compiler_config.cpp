#include "compiler_config.hpp"

#include <clap/clap.hpp>
#include <clap/param_builder.hpp>
#include <clap/exceptions.hpp>
#include <clap/help_message_generator.hpp>

namespace compiler {
	clap::Clap compilerOptions() {
		return clap::Clap()
		    .addHelpFlag()
		    .setDefaultParser(clap::FileParser::make())
		    .add(clap::ParamBuilder::ofValue(clap::StringParser::make())
		             .addShortName('o')
		             .addLongName("output")
		             .addShortDesc("Set output file")
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

		for (usize i = 0; i < parsed_args.getExtraParameterCount(); i++)
			out.files.emplace_back(parsed_args.getExtra<fs::FilePath>(i).value());

		return out;
	}

	printer::PrinterContentsSeq generateHelpMessage(const clap::exceptions::HelpException& e) {
		auto                        options{ compilerOptions() };
		printer::PrinterContentsSeq out{ { "" } };
		out.emplace_back(clap::HelpMessageGenerator::generate(options, e.parsing_result));
		return out;
	}

}
