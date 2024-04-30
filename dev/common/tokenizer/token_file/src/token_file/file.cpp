#include <base/exceptions.hpp>
#include <lexer/classifications.hpp>
#include "base/raw_view.hpp"
#include "diagnostic/logger.hpp"
#include "file.hpp"
#include "filesystem/encoding.hpp"
#include "lexer/decode.hpp"

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

	TokenFile::TokenFile(fs::FilePath&& path): path(std::move(path)) {
		content = path.getContent();
		auto log = dia::Logger();
		decoded = lexer::decode<fs::UTF8>(content->view(), log);

		usize line = 1;
		usize start = 0;
		usize newline{};
		line_begins.insert({0, 1});
		for(usize i = 0; i < content->size(); i++) {
			newline = isNewLine({decoded->begin() + (long)i, decoded->end()});
			if (newline) {
				usize end = (*decoded)[i + newline].raw_begin - (*decoded)[]
				lines.emplace_back(start, );
				line++;
				start = i + newline;
				line_begins.insert({i + newline, line});
			}
		}
		lines.emplace_back()
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