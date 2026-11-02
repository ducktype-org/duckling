// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file count_tokens_duckling.cpp
 * @author Antek Wiśniewski
 * @date 2024-09-17
 *
 * @note Outputs and returns -1 on error in code;
 */

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <printer/stream_printer.hpp>
#include <token_source/source.hpp>

#include <iostream>

int count_tokens(const lexer::Tokens& tokens);

int count_token(const lexer::Token& token) {
	using lexer::Token;
	switch (token.getType()) {
	case Token::Type::Comment:
	case Token::Type::Empty:
	case Token::Type::Error:
	case Token::Type::Sentinel:
		return 0;
	case Token::Type::BracketGroup:
		return 2 + count_tokens(token.getRecursive());
	default:
		return 1;
	}
}

int count_tokens(const lexer::Tokens& tokens) {
	int res = 0;
	for (auto& token: tokens) res += count_token(token);
	return res;
}

int main(int argc, const char** argv) {
	auto clah = clah::Clah("count_tokens_duckling_playground")
	                .addPositional(clah::FileParser::make("file"));

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

	auto path = input.getPositional<fs::File>(0);
	auto file = tokenizer::makeTokenSource(path);

	if (!file->tokenize()) {
		std::cout << -1;
		return -1;
	}

	std::cout << count_tokens(file->getTokenData().tokens);
	return 0;
}
