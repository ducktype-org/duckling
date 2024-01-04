/**
 * @file clap_cat.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include <iostream>
#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/help_message_generator.hpp>
#include <printer/printer.hpp>
#include "filesystem/file.hpp"

int main(int argc, const char** argv) {
	auto clap = clap::Clap()
	                .addHelpFlag()
	                .addPositional(clap::FileParser::make())
	                .setDefaultParser(clap::FileParser::make());
	clap::ParsingResult result;
	try {
		result = clap.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::Console console = printer::Console();
		console.add({
			{
				{ base::strConcat(argv[0], ": "), printer::Color::DEFAULT },
				{ "error: ", printer::Color::RED },
				{ e.what(), printer::Color::DEFAULT },
			},
			printer::MessageType::ERROR,
			0,
		});
		console.print(std::cerr);
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	std::cout << result.getPositional<fs::FilePath>(0).getContent().view().stdString() << '\n';

	for (usize i = 0; i < result.getExtraParameterCount(); i++)
		std::cout << result.getExtra<fs::FilePath>(i).value().getContent().view().stdString()
				  << '\n';
}
