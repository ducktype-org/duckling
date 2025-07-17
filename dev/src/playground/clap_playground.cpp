/**
 * @file clap_playground.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include <base/int_conv.hpp>

#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/help_message_generator.hpp>
#include <clap/param_builder.hpp>
#include <printer/stream_printer.hpp>

#include <iostream>

void greet(i64 n, const std::string& name) { std::cout << n << ": Hello " << name << "!\n"; }

int main(int argc, const char** argv) {
	auto clap
		= clap::Clap()
	          .addHelpFlag()
	          .setDefaultParser(clap::StringParser::make("name"))
	          .addPositional(clap::StringParser::make("name"))
	          .add(clap::ParamBuilder::ofValue(clap::RangeParser::make())
	                   .addShortName('r')
	                   .addLongName("range")
	                   .addShortDesc("Range of values to greet.")
	                   .required()
	                   .build())
	          .add(clap::ParamBuilder::ofFlag()
	                   .addShortName('d')
	                   .addLongName("descending")
	                   .addShortDesc("Whether or not numbers should be print in a descending order.")
	                   .addLongDesc("This is a multiline,\nlong comment, that should\n"
	                                "explain this parameter with more\ndetail...")
	                   .build())
	          .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
	                   .addShortName('n')
	                   .addLongName("number")
	                   .addShortDesc("Number of times to greet each person.")
	                   .build());

	clap::ParsingResult result;

	try {
		result = clap.parse(base::safeIntConv<usize>(argc), argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::StreamPrinter::print({
			{ "duckling: ", printer::Color::DEFAULT },
			{ "error: ", printer::Color::RED },
			{ e.what(), printer::Color::DEFAULT },
		});
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	std::vector<std::string> names;

	names.push_back(result.getPositional<std::string>(0));
	for (usize c = 0; c < result.getExtraParameterCount(); c++)
		names.push_back(*result.getExtra<std::string>(0));

	clap::RangeParser::Range range = *result.getValue<clap::RangeParser::Range>("range");
	auto [begin, end]              = range;
	i64 n                          = result.getValue<i64>('n').copyValueOr(1);

	for (const auto& name: names) {
		for (i64 i = 0; i < n; i++) {
			if (result.isFlag("descending"))
				for (i64 j = end - 1; j >= begin; j--) greet(j, name);
			else
				for (i64 j = begin; j < end; j++) greet(j, name);
		}
	}

	std::cout << '\n';
}
