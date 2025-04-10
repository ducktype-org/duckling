/**
 * @file count_tokens_duckling.cpp
 * @author Antek Wiśniewski
 * @date 2024-09-17
 *
 * @note Outputs and returns -1 on error in code;
 */

#include <clap/clap.hpp>
#include <filesystem/file.hpp>
#include <printer/stream_printer.hpp>
#include <token_file/file.hpp>

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
	auto clap = clap::Clap().addHelpFlag().addPositional(clap::FileParser::make("file"));

	clap::ParsingResult input;

	try {
		input = clap.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::StreamPrinter::print({
			{ "duckling: ", printer::Color::DEFAULT },
			{ "error: ", printer::Color::RED },
			{ e.what(), printer::Color::DEFAULT },
		});
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	auto path = input.getPositional<fs::FilePath>(0);
	auto file = tokenizer::makeTokenFile(path);

	if (!file->tokenize()) {
		std::cout << -1;
		return -1;
	}

	std::cout << count_tokens(file->getTokenData().tokens);
	return 0;
}
