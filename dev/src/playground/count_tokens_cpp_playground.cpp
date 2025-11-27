/**
 * @file count_tokens_cpp.cpp
 * @author Antek Wiśniewski
 * @date 2024-09-17
 *
 * @note The tool used to count tokens for c++ is a simple parser that is used for things like
 * highlighting in programs like vim so it doesn't exactly use what c++ defines as tokens. This
 * mostly matters in things like preprocessing directives, strings and comments which are fixed
 * manually for basic cases.
 *
 * @note Outputs and returns -1 on error in code;
 */

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <printer/stream_printer.hpp>

#include <tree-sitter-cpp.h>
#include <tree_sitter/api.h>

#include <iostream>
#include <regex>

const std::regex is_string_literal{ R"--(^.*string_literal$)--" };
const std::regex is_preproc{ R"--(^preproc_.*$)--" };

/**
 * @note Includes are hardcoded to have 3 tokens which is more aligned with c++ definitions
 * @note Strings are hardcoded to always be 1 token which is more aligned with c++ definitions
 * @note Comments are hardcoded to always be 0 tokens which is more aligned with c++ definitions
 */
int countTokens(TSNode node) {
	u32         count = ts_node_child_count(node);
	std::string type{ ts_node_type(node) };
	// std::cout << type << "\n";
	if (ts_node_is_extra(node)) {
		// std::cout << type << "\n";
		return 0;
	} else if (count == 0 || std::regex_match(type, is_string_literal)) {
		// std::cout << type << "\n";
		return 1;
	} else if ("preproc_include" == type) {
		// std::cout << type << "\n";
		return 3;
	} else if (std::regex_match(type, is_preproc)) {
		std::cerr << "Non include preprocessing directives are untested for token counts";
		// This is an educated guess of how it would be treated
		int res = 1;
		for (u32 child = 0; child < count; child++) res += countTokens(ts_node_child(node, child));
		return res;
	} else {
		// std::cout << type << "\n";
		int res = 0;
		for (u32 child = 0; child < count; child++) res += countTokens(ts_node_child(node, child));
		return res;
	}
}

struct CppParser {
	const std::string source_code;
	TSParser*         parser;
	TSTree*           tree;

	CppParser(const std::string& source_code): source_code(source_code), parser(ts_parser_new()) {
		if (ts_parser_set_language(parser, tree_sitter_cpp()))
			tree = ts_parser_parse_string(
				parser, nullptr, source_code.data(), base::safeIntConv<u32>(source_code.size())
			);
		else
			CORE_PANIC("Failed to set parser language.");
	}

	int getTokenCount() {
		TSNode root_node = ts_tree_root_node(tree);
		if (ts_node_has_error(root_node)) return -1;
		return countTokens(root_node);
	}

	~CppParser() {
		ts_tree_delete(tree);
		ts_parser_delete(parser);
	}
};

int main(int argc, const char** argv) {
	auto clah
		= clah::Clah("count_tokens_cpp_playground").addPositional(clah::FileParser::make("file"));

	clah::ParsingResult input;

	try {
		input = clah.parse(base::safeIntConv<usize>(argc), argv);
	} catch (clah::exceptions::ClahException& e) {
		printer::StreamPrinter::print({
			{ "duckling: ", printer::Color::Default },
			{ "error: ", printer::Color::Red },
			{ e.what(), printer::Color::Default },
		});
		return 1;
	} catch (clah::exceptions::HelpException& e) {
		std::string help_message = clah::HelpMessageGenerator::generate(clah, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	auto              path          = input.getPositional<fs::File>(0);
	auto              file_contents = path.getContent();
	const std::string source_code   = file_contents.view().stdString();

	CppParser parser(source_code);
	int       token_count = parser.getTokenCount();
	std::cout << parser.getTokenCount() << "\n";

	if (token_count == -1) return -1;
	return 0;
}
