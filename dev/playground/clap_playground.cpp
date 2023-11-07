/**
 * @file parser_testing.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include <iostream>
#include <clap/clap.hpp>
#include <clap/param_builder.hpp>
#include <clap/exceptions.hpp>
#include <printer/printer.hpp>

int main(int argc, const char** argv) {
	auto clapper
		= clap::Clap()
	          .setDefaultParser(clap::StringParser::make())
	          .add(clap::ParamBuilder::ofValue(clap::RangeParser::make())
	                   .addShortName('r')
	                   .addShortDesc("Range of values to greet.")
	                   .required()
	                   .build())
	          .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
	                   .addShortName('n')
	                   .addLongName("number")
	                   .addShortDesc("Number of times to greet each person.")
	                   .build())
	          .add(clap::ParamBuilder::ofFlag()
	                   .addShortName('d')
	                   .addLongName("descending")
	                   .addShortDesc("Whether or not numbers should be print in a descending order")
	                   .build());

	clap::ParsingResult result;

	try {
		result = clapper.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::Console console = printer::Console();
		console.add({
			{
				{ "rift: ", printer::Color::DEFAULT },
				{ "error: ", printer::Color::RED },
				{ e.what(), printer::Color::DEFAULT },
			},
			printer::MessageType::ERROR,
			0,
		});
		console.print(std::cerr);
		return 1;
	}

	auto range = *result.getValue<clap::RangeParser::Range>('r');
	i64  start = range.begin;
	i64  end   = range.end;
	if (start > end)
		for (i64 i = start; i > end; i--) std::cout << i << ' ';

	else
		for (i64 i = start; i < end; i++) std::cout << i << ' ';
	std::cout << '\n';


	for (usize c = 0; c < result.getExtraParameterCount(); c++) {
		i64  times = result.getValue<i64>("number").value_or(3);
		auto name  = *result.getExtra<std::string>(c);
		if (result.isFlag("descending"))
			for (i64 i = times; i >= 1; i--) std::cout << i << ": " + name << ' ';

		else
			for (i64 i = 1; i <= times; i++) std::cout << i << ": " + name << ' ';
		std::cout << '\n';
	}
}
