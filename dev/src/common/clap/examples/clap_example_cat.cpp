// Prints n (n is optional) times contents of a given file(s) to stdin.
// Usage:   cat <file> [file...]
// Example: cat foo.txt -n 5

#include <clap/clap.hpp>
#include <filesystem/file.hpp>

#include <base/int_conv.hpp>

#include <iostream>

void printFile(const fs::FilePath& file) {
	std::cout << file.getContent().view().stdString() << '\n';
}

void catFile(const clap::ParsingResult& result) {
	i64 times = result.getValue<i64>('n').valueOr(1);
	while (times--) {
		// Since we are using clap::FileParser, it automatically links
		// specified input to real files!
		auto file1 = result.getPositional<fs::FilePath>(0);
		printFile(file1);

		// Finally, iterate over 'extra' parameters and print them out as well.
		// getExtra returns a base::Optional<T>, but we know it has a value.
		for (usize i = 0; i < result.getExtraParameterCount(); i++)
			printFile(*result.getExtra<fs::FilePath>(i));
	}
}

int main(int argc, const char** argv) {
	// Create a clap object and set value parsers.
	auto clap = clap::Clap("prog")
	                .addHelpFlag()
	                .addPositional(clap::FileParser::make())
	                // "another_file" is an optional name for the parameter's value.
	                // Displays in i.e. a help message.
	                .setDefaultValueParser(clap::FileParser::make("another_file"))
	                // This will invoke if execute() is being used.
	                .setHandler([](const clap::ParsingResult& result) -> int {
						catFile(result);
						return 0;
					})
	                .add(
						clap::ParamBuilder::ofValue(clap::IntParser::make())
							.optional()  // It's the default
							.addShortName('n')
							.addLongName("times")
							.addShortDesc("How many times to print each content")
							.build()
					);

	// .addPositional(FileParser) tells clap to expect at least one file.

	// Old usage.
	std::cout << "Usage with parse:\n";
	try {
		// Real parsing happens here. Only this operation may throw clap exception.
		const clap::ParsingResult result = clap.parse(base::safeIntConv<usize>(argc), argv);
		catFile(result);
	} catch (clap::exceptions::HelpException& help) {
		// Since we added a built-in help flag, we can catch a help exception.
		// There is a useful generic HelpMessageGenerator.
		std::string help_msg = clap::HelpMessageGenerator::generate(clap, help.parsing_result);
		std::cout << help_msg << '\n';
		return 0;
	} catch (clap::exceptions::ClapException& e) {
		// In case of any other exception, user did something wrong.
		std::cerr << "ERROR: " << e.what() << '\n';
		return 1;
	}

	// New usage. Automatically invoke the handlers, catch exceptions and print help messages.
	std::cout << "Usage with execute:\n";
	clap.execute(base::safeIntConv<usize>(argc), argv);
}
