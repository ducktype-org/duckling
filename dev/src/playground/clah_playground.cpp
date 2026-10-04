/**
 * @file clah_playground.cpp
 * @author Piotr Oleszczuk (poleszczuk3@gmail.com)
 * @brief Quick demonstration of `Clah`.
 *
 * This file showcases the capabilities of the `Clah` library by building a sample
 * command-line application with subcommands, global options, and various parameter types.
 *
 * A few examples:
 * The basic structure is:
 *   ./clah_playground [GLOBAL OPTIONS] <COMMAND> [COMMAND-SPECIFIC ARGUMENTS & OPTIONS]
 *
 * Type `./clah_playground --help` to list all the global parameters and subcommands.
 * Type `./clah_playground [COMMAND] --help` to list the subcommand specific parameters and global
 * flags
 *
 * === Global Options ===
 * These options can be used with any command:
 *   --user <name>   (Required) Specifies the user running the command.
 *   -v, --verbose   (Optional) Enables verbose output.
 *
 * === Subcommand: `greet` ===
 * This command greets one or more people, with several customization options.
 * 1.  Minimal required usage:
 *     $ ./clah_playground --user Alice greet --range 1..3 "Bob"
 *     > Running as user: Alice
 *     > 1: Hello, Bob!
 *     > 2: Hello, Bob!
 *
 * 2.  Using optional parameters (`--times` and `--descending`):
 *     $ ./clah_playground --user Bob greet --range 5..8 --times 2 --descending World
 *     > Running as user: Bob
 *     > 7: Hello, World!
 *     > 6: Hello, World!
 *     > 5: Hello, World!
 *     > 7: Hello, World!
 *     > ... (greets twice in descending order)
 *
 * 3.  Providing multiple names (positional and extra arguments):
 *     The first name is a required positional argument. Subsequent names are captured
 *     as extra arguments.
 *     $ ./clah_playground --user David greet --range 0..1 Alice Bob Charlie
 *     > Running as user: David
 *     > 0: Hello, Alice!
 *     > 0: Hello, Bob!
 *     > 0: Hello, Charlie!
 *
 *
 * === Subcommand: `Config` ===
 * This command demonstrates how global options affect different parts of the application.
 *
 * 1.  Basic usage:
 *     $ ./clah_playground --user Eve config
 *     > Hi user: Eve
 *     > Configuration:
 *     >   Verbose mode: false
 *     >   Running as user: Eve
 *
 * 2.  With the global `--verbose` flag:
 *     $ ./clah_playground --user Frank --verbose config
 *     > Hi user: Frank
 *     > Configuration:
 *     >   Verbose mode: true
 *     >   Running as user: Frank
 *
 * @note: The flags are binding to last typed subcommand, which means:
 * The line below is correct:
 * $ ./clah_playground --user Alice greet --range 1..3 "Bob"
 * The line below is incorrect:
 * $ ./clah_playground --user Alice --range 1..3 greet "Bob"
 *
 * Global flags are an exception and can be types anywhere.
 */
#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <printer/stream_printer.hpp>

#include <iostream>
#include <string>

void showConfig(const clah::ParsingResult& result) {
	std::cout << "Configuration:\n";
	std::cout << "  Verbose mode: " << std::boolalpha << result.isFlag("verbose") << "\n";
	if (auto user = result.getValue<std::string>("user"))
		std::cout << "  Running as user: " << user.value() << "\n";
	else
		std::cout << "  Running as default user.\n";
}

void greet(i64 n, const std::string& name) { std::cout << n << ": Hello, " << name << "!\n"; }

// Just a helper to avoid duplication.
void multiGreeter(const clah::ParsingResult& result) {
	if (result.isFlag("verbose")) std::cout << "Verbose mode is on for 'greet'.\n";

	std::vector<std::string> names;
	names.push_back(result.getPositional<std::string>(0));
	for (usize i = 0; i < result.getExtraParameterCount(); ++i)
		names.push_back(result.getExtra<std::string>(i).value());

	clah::RangeParser::Range range = *result.getValue<clah::RangeParser::Range>("range");
	auto [begin, end]              = range;
	i64 n                          = result.getValue<i64>("times").copyValueOr(1);

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

clah::Clah getClahForPlayground() {
	return clah::Clah("clah_playground", "A cool cli app to demonstrate Clah.")
	    .setDefaultValueParser(clah::StringParser::make("extra_names"))
	    .setPreHandler([](const clah::ParsingResult& result) {
			std::cout << "This is a function which get's invoked before all other handlers. It may "
						 "be usefull to configure some global state like lexer options.\n";
			if (result.getValue<std::string>("user").has_value())
				std::cout << "Running as user: " << result.getValue<std::string>("user").value()
						  << '\n';
			else
				std::cout << "Hi default user\n";
		})
	    .add(  // Add a global parameter (visible in all subcommands).
			clah::ParamBuilder::ofFlag()
				.addShortName('v')
				.addLongName("verbose")
				.addShortDesc("Enable verbose output for all commands.")
				.build()
		)
	    .add(clah::ParamBuilder::ofValue(clah::StringParser::make())
	             .addLongName("user")
	             .addShortDesc("Run command as a specific user.")
	             .required()
	             .build())
	    .addSubcommand(                                           // `playground greet` subcommand.
			clah::Clah("greet", "Greets one or more people.")
				.addPositional(clah::StringParser::make("name"))  // Positional argument.
				.add(clah::ParamBuilder::ofValue(clah::RangeParser::make())
	                     .addShortName('r')
	                     .addLongName("range")
	                     .addShortDesc("Range of values to greet.")
	                     .required()
	                     .build())
				.add(  // Optional parameter.
					clah::ParamBuilder::ofValue(clah::IntParser::make())
						.addShortName('n')
						.addLongName("times")
						.addShortDesc("Number of times to greet.")
						.build()
				)
				.add(clah::ParamBuilder::ofFlag()
	                     .addShortName('d')
	                     .addLongName("descending")
	                     .addShortDesc(
							 "Whether or not numbers should be print in a descending order."
						 )
	                     .addLongDesc("This is a multiline,\nlong comment, that should\n"
	                                  "explain this parameter with more\ndetail...")
	                     .build())
				// A handler for this subcommand, which will be invoked when
	            // clah.execute(argc, argv) is executed.
				.setHandler([](const clah::ParsingResult& result) -> int {
					multiGreeter(result);
					return 0;
				})
		)
	    .addSubcommand(clah::Clah("config", "Displays the current configuration.")
	                       .setHandler([](const clah::ParsingResult& result) -> int {
							   showConfig(result);
							   return 0;
						   }));
}

int main(int argc, const char** argv) {
	auto clah = getClahForPlayground();

	std::cout << "------- USING CLAP VIA THE EXECUTE() FUNCTION ---------\n";
	try {
		// This parses the arguments and invokes the handlers of matched subcommands.
		clah.execute(base::safeIntConv<usize>(argc), argv);
	}
	// The HelpException is caught in the execute function, so it's CLAPs role to print the help.
	// catch (const clah::exceptions::HelpException& e) {
	// 	std::cout << clah::HelpMessageGenerator::generate(clah, e.parsing_result) << '\n';
	// 	return 0;
	// }
	catch (const clah::exceptions::ClahException& e) {
		printer::StreamPrinter::print({
			{ "playground: ", printer::Color::Default },
			{ "error: ", printer::Color::Red },
			{ e.what(), printer::Color::Default },
		});
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "An unexpected error occurred: " << e.what() << '\n';
		return 1;
	}

	// Besides the execute function you can still use the old parse function.
	std::cout << "\n------- USING CLAP VIA THE PARSE() FUNCTION LIKE SAME AS BEFORE ---------\n";
	clah::ParsingResult result;
	try {
		result = clah.parse(base::safeIntConv<usize>(argc), argv);
	} catch (clah::exceptions::ClahException& e) {
		printer::StreamPrinter::print({
			{ "duckling: ", printer::Color::Default },
			{ "error: ", printer::Color::Red },
			{ e.what(), printer::Color::Default },
		});
		return 1;
	} catch (clah::exceptions::HelpException& e) {
		// If using parse() you need to handle the HelpException yourself.
		std::string help_message = clah::HelpMessageGenerator::generate(clah, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	multiGreeter(result);
}
