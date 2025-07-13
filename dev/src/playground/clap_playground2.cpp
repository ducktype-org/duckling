/**
 * @file clap_playground.cpp
 * @author Piotr Oleszczuk (poleszczuk3@gmail.com)
 */
#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/help_message_generator.hpp>
#include <printer/stream_printer.hpp>

#include <base/int_conv.hpp>

#include <iostream>
#include <string>

void greet(i64 n, const std::string& name) { std::cout << n << ": Hello, " << name << "!\n"; }

void showConfig(const clap::ParsingResult& result) {
	std::cout << "Configuration:\n";
	std::cout << "  Verbose mode: " << std::boolalpha << result.isFlag("verbose") << "\n";
	if (auto user = result.getValue<std::string>("user"))
		std::cout << "  Running as user: " << user.value() << "\n";
	else
		std::cout << "  Running as default user.\n";
}

int main(int argc, const char** argv) {
	auto clap
		= clap::Clap("playground", "A cool cli app to demonstrate Clap.")
	          .addHelpFlag()        // Automatically add a global help parameter.
	          .addGlobalParameter(  // Add a global parameter (available in all subcommands).
				  clap::ParamBuilder::ofFlag()
					  .addShortName('v')
					  .addLongName("verbose")
					  .addShortDesc("Enable verbose output for all commands.")
					  .build()
			  )
	          .addGlobalParameter(
				  clap::ParamBuilder::ofValue(clap::StringParser::make())
					  .addLongName("user")
					  .addShortDesc("Run command as a specific user.")
					  .build()
			  )
	          .addSubcommand(  // playground greet subcommand.
				  clap::Command("greet", "Greets one or more people.")
					  .addPositional(clap::StringParser::make("name"))  // Positional argument.
					  .add(                                             // Optional parameter.
						  clap::ParamBuilder::ofValue(clap::IntParser::make())
							  .addShortName('n')
							  .addLongName("times")
							  .addShortDesc("Number of times to greet.")
							  .build()
					  )
					  .add(  // flag
						  clap::ParamBuilder::ofFlag()
							  .addShortName('e')
							  .addLongName("excited")
							  .addShortDesc("Show extra excitement!")
							  .build()
					  )
					  // Default parser for additional arguments.
					  .setDefaultValueParser(clap::StringParser::make("extra_names"))
					  // A handler for this subcommand, which will be invoked when
	                  // clap.execute(argc, argv) is executed.
					  .setHandler([](const clap::ParsingResult& result) -> int {
						  if (result.isFlag("verbose"))
							  std::cout << "Verbose mode is on for 'greet'.\n";

						  std::vector<std::string> names;
						  names.push_back(result.getPositional<std::string>(0));
						  for (usize i = 0; i < result.getExtraParameterCount(); ++i)
							  names.push_back(result.getExtra<std::string>(i).value());

						  i64  times   = result.getValue<i64>("times").valueOr(1);
						  bool excited = result.isFlag("excited");

						  for (i64 i = 0; i < times; ++i) {
							  for (const auto& name: names) {
								  std::cout << "Hello, " << name;
								  if (excited) std::cout << "!!!";
								  std::cout << "\n";
							  }
						  }
						  return 0;
					  })
			  )
	          .addSubcommand(
				  clap::Command("config", "Displays the current configuration.")
					  .setHandler([](const clap::ParsingResult& result) -> int {
						  showConfig(result);
						  return 0;
					  })
			  );

	try {
		return clap.execute(usize(argc), argv);
	} catch (const clap::exceptions::HelpException& e) {
		std::cout << clap::HelpMessageGenerator::generate(clap, e.parsing_result) << '\n';
		return 0;
	} catch (const clap::exceptions::ClapException& e) {
		printer::StreamPrinter::print(
			{
				{ "playground: ", printer::Color::DEFAULT },
				{ "error: ", printer::Color::RED },
				{ e.what(), printer::Color::DEFAULT },
			}
		);
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "An unexpected error occurred: " << e.what() << '\n';
		return 1;
	}
}
