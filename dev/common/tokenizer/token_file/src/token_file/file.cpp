#include "file.hpp"
#include <base/exceptions.hpp>
#include <lexer/classifications.hpp>
#include "base/raw_view.hpp"
#include "filesystem/encoding.hpp"
#include "lexer/decode.hpp"
#include "lexer/lexer_class.hpp"

namespace tokenizer {
	/**
	 * @brief Checks for newline at the beginning of a set of characters.
	 *
	 * @note It's only CR + LF that combined are only a single newline
	 *
	 * @return usize - 0 if there is new line, otherwise number of characters that together form a
	 * newline
	 */
	usize isNewLine(const std::span<lexer::Char> where) {
		auto& newline = lexer::Classifications::newline;
		if (where.size() > 0 && where[0].is(newline)) {
			if (where.size() > 1 && where[0].is(0x0D) && where[1].is(0x0A)) return 2;
			return 1;
		}
		return 0;
	}

	TokenFile::TokenFile(const fs::FilePath& path): path(path) { content = path.getContent(); }

	void TokenFile::countLines() {
		if (log.bad()) return;
		usize line  = 1;
		usize start = 0;
		usize newline{};

		line_begins.insert({ 0, 1 });
		for (usize i = 0; i < decoded->size(); i++) {
			newline = isNewLine({ decoded->begin() + (long) i, decoded->end() });
			if (newline) {
				lines.emplace_back(start, i);
				line++;
				start = i + newline;
				line_begins.insert({ i + newline, line });
				i += newline - 1;
			}
		}
		// Last line without EOF
		lines.emplace_back(start, decoded->size() - 1);
	}

	std::pair<usize, usize> TokenFile::getLineColumn(usize source_pos) {
		auto  line_it = --line_begins.lower_bound({ source_pos, -1 });
		usize line    = line_it->second;
		usize col     = source_pos - line_it->first + 1;
		return { line, col };
	}

	base::RawView TokenFile::getCharRange(usize begin_char, usize end_char) {
		//@TODO: add checks
		base::RawArray begin = content->view().getBegin() + decoded->at(begin_char).index;
		usize          size  = decoded->at(end_char).index - decoded->at(begin_char).index;
		return { begin, size };
	}

	dia::Logger& TokenFile::getLogger() { return log; }

	void TokenFile::runLexer() {
		if (log.bad()) return;
		lexer::Lexer lexer{ self };
		token_data = lexer.tokenize();
	}

	fs::FileContent TokenFile::getContent() {
		if (content) return content.value();
		return path.getContent();
	}

	fs::FilePath TokenFile::getPath() { return path; }

	lexer::CharArray& TokenFile::getChars() {
		if (!decoded) RIFT_PANIC("Tried to access nonexistant Character data.");
		return decoded.value();
	}

	lexer::TokenData& TokenFile::getTokenData() {
		// @TODO: Maybe use lexer to create it.
		if (!token_data) RIFT_PANIC("Tried to access nonexistant token data.");
		return token_data.value();
	}
}
