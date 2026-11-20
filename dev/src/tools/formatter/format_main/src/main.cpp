// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file main.cpp
 * @brief Entry point of `duckfmt`, the standalone Duckling source formatter.
 *
 * Reads one or more Duckling source files, re-renders each through the
 * token-based formatter and prints the result to stdout (or writes it back with
 * -i, or verifies it with -k). Unlike `duckc`, this binary does not initialize the
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
#include <string_view>
#include <vector>

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

	/**
	 * @brief Formats one source file and prints, writes back or checks the result.
	 *
	 * @param print_header Print a `==> <path> <==` line before the formatted source, so the
	 *        output of several files on stdout can be told apart.
	 * @return `true` on success; `false` when the file could not be tokenized or formatted,
	 *         or when it is not formatted in check mode (the reason is printed on stderr).
	 */
	bool formatOneFile(
		fs::File&                      file_to_format,
		const formatter::FormatConfig& format_config,
		const clah::ParsingResult&     options,
		bool                           print_header
	) {
		// A copy: getFilePath() returns a temporary, a view of it would dangle.
		const std::string path(file_to_format.getFilePath().strView());

		auto       source_content = file_to_format.getContent();
		const auto source_view    = source_content.view().stringView();

		auto token_source = tokenizer::makeTokenSource(file_to_format);
		// Comments must stay in the token stream, or formatting would erase them.
		if (not token_source->tokenize({ .keep_comments = true })) {
			token_source->getIntLogger()->terminalPrint(std::cerr);
			return false;
		}

		std::string formatted;
		try {
			formatted = formatter::formatTokens(token_source->getTokenData(), format_config);
		} catch (const base::Exception& e) {
			std::cerr << path << ": failed to format: " << e.what() << "\n";
			return false;
		}

		if (options.isFlag("check")) {
			if (formatted == source_view) return true;
			// On stderr, so `duckfmt -k` stays usable in a pipeline.
			std::cerr << path << ": not formatted\n";
			return false;
		}

		if (options.isFlag("in-place")) {
			file_to_format.writeToFile(formatted);
			return true;
		}

		if (print_header) std::cout << "==> " << path << " <==\n";
		std::cout << formatted;
		return true;
	}

	/**
	 * @brief Handler of `duckfmt`: formats every file given on the command line.
	 *
	 * All files are processed even when one of them fails.
	 *
	 * @return 0 when every file succeeded, 1 otherwise.
	 */
	int formatFiles(const clah::ParsingResult& options) {
		formatter::FormatConfig format_config = formatter::FormatConfig::defaults();

		const auto config_path = options.getValue<std::string>("config").copyValueOr("");
		if (not config_path.empty()) {
			const auto loaded = loadConfig(config_path);
			if (not loaded.has_value()) return 1;
			format_config = loaded.value();
		}

		std::vector<fs::File> files_to_format;
		files_to_format.push_back(options.getPositional<fs::File>(0));
		for (usize i = 0; i < options.getExtraParameterCount(); i++)
			files_to_format.push_back(options.getExtra<fs::File>(i).value());

		const bool print_headers = files_to_format.size() > 1;
		bool       all_ok        = true;
		for (usize i = 0; i < files_to_format.size(); i++) {
			// A blank line between two files keeps the headers easy to spot.
			if (print_headers and i > 0 and not options.isFlag("check")
			    and not options.isFlag("in-place"))
				std::cout << "\n";
			if (not formatOneFile(files_to_format[i], format_config, options, print_headers))
				all_ok = false;
		}
		return all_ok ? 0 : 1;
	}

	clah::Clah getClahForDuckfmt() {
		constexpr std::string_view DESCRIPTION
			= "Formats Duckling source files and prints the result to cout. With several files, "
			  "each output starts with a `==> <file> <==` line.";
		return clah::Clah("duckfmt", std::string(DESCRIPTION))
		    .addPositional(clah::FileParser::make("file"))
		    .setDefaultValueParser(clah::FileParser::make("file"))
		    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("config"))
		             .addShortName('c')
		             .addLongName("config")
		             .addShortDesc("Path to a JSON formatter configuration file.")
		             .optional()
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('i')
		             .addLongName("in-place")
		             .addShortDesc("Write the result back to the files instead of stdout.")
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('k')
		             .addLongName("check")
		             .addShortDesc(
						 "Do not rewrite the files; report each unformatted file on stderr and "
						 "exit non-zero if any of them is not already formatted."
					 )
		             .build())
		    .setHandler(formatFiles);
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
