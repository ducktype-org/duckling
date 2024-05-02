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
		for(usize i = 0; i < content->size(); i++) {
			newline = isNewLine({decoded->begin() + (long)i, decoded->end()});
			if (newline) {
				lines.emplace_back(start, i + newline - 1);
				line++;
				start = i + 1;
				line_begins.insert({i + newline, line});
			}
		}
		// Last line without EOF
		lines.emplace_back(start, content->size() - 2);
	}

	dia::Logger& TokenFile::getLogger() {
		return log;	
	}

	void TokenFile::runLexer() {
		if (log.bad()) return;
		lexer::Lexer lexer{path, log};
		token_data = lexer.tokenize();
	}

	fs::FileContent TokenFile::getContent() {
		if (content) return content.value();
		return path.getContent();
	}

	lexer::TokenData& TokenFile::getTokenData() {
		// @TODO: Maybe use lexer to create it.
		if (!token_data) {
			RIFT_PANIC("Tried to access nonexistant token data.");
		}
		return token_data.value();
	}
}