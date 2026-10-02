/**
 * @file main.cpp
 * @brief Entry point of `duckfmt`, the standalone Duckling source formatter.
 *
 * Reads a single Duckling source file, re-renders it through the token-based
 * formatter and prints the result to stdout (or writes it back with -i, or
 * verifies it with -k). Unlike `duckc`, this binary does not initialize the
 * compiler driver: tokenization and formatting need no query state.
 */

#include <formatter/config.hpp>
#include <formatter/formatter.hpp>

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>
#include <token_source/source.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

namespace {

	/**
	 * @brief Loads the formatter configuration from @p config_path.
	 *
	 * @return The parsed configuration, or an empty optional after printing an error when the
	 *         file is missing or holds invalid JSON.
	 */
	base::Optional<formatter::FormatConfig> loadConfig(const std::string& config_path) {
		// fs::File's constructor panics on a missing path, so validate first.
		fs::FilePath config_file_path(config_path);
		if (not config_file_path.exists()) {
			std::cerr << "Error: formatter config file does not exist: " << config_path << "\n";
			return {};
		}
		auto config_content = fs::File(config_file_path).getContent();
		// As chars, not raw bytes: libc++ has no std::char_traits<std::byte> for the parser.
		const auto content_view = config_content.view().stringView();
		try {
			auto config_json = nlohmann::json::parse(content_view.begin(), content_view.end());
			return formatter::FormatConfig::fromJson(config_json);
		} catch (const nlohmann::json::exception& e) {
			std::cerr << "Error: failed to parse formatter config JSON: " << e.what() << "\n";
			return {};
		}
	}

	int formatFile(const clah::ParsingResult& options) {
		auto file_to_format = options.getPositional<fs::File>(0);

		formatter::FormatConfig format_config = formatter::FormatConfig::defaults();

		const auto config_path = options.getValue<std::string>("config").copyValueOr("");
		if (not config_path.empty()) {
			const auto loaded = loadConfig(config_path);
			if (not loaded.has_value()) return 1;
			format_config = loaded.value();
		}

		auto       source_content = file_to_format.getContent();
		const auto source_view    = source_content.view().stringView();

		auto token_source = tokenizer::makeTokenSource(file_to_format);
		// Comments must stay in the token stream, or formatting would erase them.
		if (not token_source->tokenize({ .keep_comments = true })) {
			token_source->getIntLogger()->terminalPrint(std::cerr);
			return 1;
		}

		const auto formatted = formatter::formatTokens(token_source->getTokenData(), format_config);

		if (options.isFlag("check")) {
			if (formatted == source_view) return 0;
			// On stderr, so `duckfmt -k` stays usable in a pipeline.
			std::cerr << file_to_format.getFilePath().strView() << ": not formatted\n";
			return 1;
		}

		if (options.isFlag("in-place"))
			file_to_format.writeToFile(formatted);
		else
			std::cout << formatted;

		return 0;
	}

	clah::Clah getClahForDuckfmt() {
		return clah::Clah(
				   "duckfmt", "Formats a single Duckling source file and prints the result to cout."
		)
		    .addPositional(clah::FileParser::make("file"))
		    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("config"))
		             .addShortName('c')
		             .addLongName("config")
		             .addShortDesc("Path to a JSON formatter configuration file.")
		             .optional()
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('i')
		             .addLongName("in-place")
		             .addShortDesc("Write the result back to the file instead of stdout.")
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('k')
		             .addLongName("check")
		             .addShortDesc(
						 "Do not rewrite the file; report on stderr and exit non-zero if it is "
						 "not already formatted."
					 )
		             .build())
		    .setHandler(formatFile);
	}
}

int main(int argc, const char* argv[]) {
	init::InitObject _;

	auto clah = getClahForDuckfmt();

	try {
		return clah.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Formatter Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception not inheriting from std::exception was "
		      "caught.\n",
		      printer::Color::Default },
		});
		return 1;
	}
}
