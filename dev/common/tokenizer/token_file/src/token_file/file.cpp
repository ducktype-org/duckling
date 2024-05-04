#include "file.hpp"
#include <base/exceptions.hpp>
#include <lexer/classifications.hpp>
#include "base/raw_view.hpp"
#include "filesystem/encoding.hpp"
#include "lexer/decode.hpp"
#include "lexer/lexer_class.hpp"

namespace tokenizer {
	usize isNewLine(const std::span<lexer::Char> where) {
		auto& newline = lexer::Classifications::newline;
		if (where.size() > 0 && where[0].is(newline)) {
			if (where.size() > 1 && where[0].is(0x0D) && where[1].is(0x0A)) {
				return 2;
			}
			return 1;
		}
		return 0;
	}

	TokenFile::TokenFile(const fs::FilePath& path): path(path) {
		content = path.getContent();
	}
	void TokenFile::decode() {
		decoded = lexer::decode<fs::UTF8>(content->view(), log);
	}

	void TokenFile::countLines() {
		if (log.bad()) return;
		usize line = 1;
		usize start = 0;
		usize newline{};
		
		line_begins.insert({0, 1});
		for(usize i = 0; i < decoded->size(); i++) {
			newline = isNewLine({decoded->begin() + (long)i, decoded->end()});
			if (newline) {
				lines.emplace_back(start, i);
				line++;
				start = i + newline;
				line_begins.insert({i + newline, line});
			}
		}
		// Last line without EOF
		lines.emplace_back(start, decoded->size() - 1);
	}

	std::pair<usize, usize> TokenFile::getLineColumn(usize source_pos) {
		auto line_it = --line_begins.lower_bound({source_pos, -1});
		usize line = line_it->second;	
		usize col = source_pos - line_it->first + 1;
		return {line, col};
	}

	dia::Logger& TokenFile::getLogger() {
		return log;	
	}

	void TokenFile::runLexer() {
		if (log.bad()) return;
		lexer::Lexer lexer{self};
		token_data = lexer.tokenize();
	}

	fs::FileContent TokenFile::getContent() {
		if (content) return content.value();
		return path.getContent();
	}

	fs::FilePath TokenFile::getPath() {
		return path;
	}

	lexer::CharArray& TokenFile::getChars() {
		if (!decoded) {
			RIFT_PANIC("Tried to access nonexistant Character data.");
		}
		return decoded.value();
	}

	lexer::TokenData& TokenFile::getTokenData() {
		// @TODO: Maybe use lexer to create it.
		if (!token_data) {
			RIFT_PANIC("Tried to access nonexistant token data.");
		}
		return token_data.value();
	}

	void TokenFile::tokenize() {
		lexer::Classifications::init();
		decode();
		countLines();
		runLexer();
	}
}