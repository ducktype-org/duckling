// Prints n (n is optional) times contents of a given file(s) to stdin.
// Usage:   cat <file> [file...]
// Example: cat foo.txt -n 5
#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>

#include <iostream>

void printFile(const fs::File& file) { std::cout << file.getContent().view().stdString() << '\n'; }

void catFile(const clah::ParsingResult& result) {
	i64 times = result.getValue<i64>('n').copyValueOr(1);
	while (times--) {
		// Since we are using clah::FileParser, it automatically links
		// specified input to real files!
		auto file1 = result.getPositional<fs::File>(0);
		printFile(file1);

		// Finally, iterate over 'extra' parameters and print them out as well.
		// getExtra returns a base::Optional<T>, but we know it has a value.
		for (usize i = 0; i < result.getExtraParameterCount(); i++)
			printFile(*result.getExtra<fs::File>(i));
	}
}

int main(int argc, const char** argv) {
	// Create a clah object and set value parsers.
	auto clah = clah::Clah("clah_example_cat")
	                .addPositional(clah::FileParser::make("file"))
	                // "another_file" is an optional name for the parameter's value.
	                // Displays in i.e. a help message.
	                .setDefaultValueParser(clah::FileParser::make("another_file"))
	                // This will invoke if execute() is being used.
	                .setHandler([](const clah::ParsingResult& result) -> int {
						catFile(result);
						return 0;
					})
	                .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	                         .optional()  // It's the default
	                         .addShortName('n')
	                         .addLongName("times")
	                         .addShortDesc("How many times to print each content")
	                         .build());

	// .addPositional(FileParser) tells clah to expect at least one file.

	// Old usage.
	std::cout << "Usage with parse:\n";
	try {
		// Real parsing happens here. Only this operation may throw clah exception.
		const clah::ParsingResult result = clah.parse(base::safeIntConv<usize>(argc), argv);
		catFile(result);
	} catch (clah::exceptions::HelpException& help) {
		// Since we added a built-in help flag, we can catch a help exception.
		// There is a useful generic HelpMessageGenerator.
		std::string help_msg = clah::HelpMessageGenerator::generate(clah, help.parsing_result);
		std::cout << help_msg << '\n';
		return 0;
	} catch (clah::exceptions::ClahException& e) {
		// In case of any other exception, user did something wrong.
		std::cerr << "ERROR: " << e.what() << '\n';
		return 1;
	}

	// New usage. Automatically invoke the handlers, catch exceptions and print help messages.
	std::cout << "Usage with execute:\n";
	clah.execute(base::safeIntConv<usize>(argc), argv);
}
