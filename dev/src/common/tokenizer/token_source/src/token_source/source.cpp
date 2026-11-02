// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "source.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/shared_view.hpp>

#include <diagnostic/location.hpp>
#include <lexer/lexer_class.hpp>
#include <unicode_classification/classifications.hpp>

namespace tokenizer {
	/**
	 * @brief Checks for newline at the beginning of a set of characters.
	 *
	 * @note It's only CR + LF that combined are only a single newline
	 *
	 * @return usize - 0 if there is new line, otherwise number of characters that together form a
	 * newline
	 */
	usize isNewLine(const std::span<const lexer::Char> where) {
		auto& newline = unicode::Classifications::newline;
		if (where.size() > 0 && where[0].is(newline)) {
			if (where.size() > 1 && where[0].is(0x0D) && where[1].is(0x0A)) return 2;
			return 1;
		}
		return 0;
	}

	TokenSource::TokenSource(const fs::File& path):
		  location(makeBox<dia::FileLocation>(Ref<TokenSource>(this), path)) {
		content.emplace(path.getContent());
	}

	TokenSource::TokenSource(dia::StablePosition parent, const std::string_view contents):
		  location(makeBox<dia::MacroLocation>(parent, Ref<TokenSource>(this))) {
		content.emplace(base::SharedView::copy(base::RawView{
			reinterpret_cast<const byte*>(contents.data()), contents.size() }));
	}

	void TokenSource::countLines() {
		if (int_log.hasErrors()) return;
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

	std::pair<usize, usize> TokenSource::getLineColumn(usize source_pos) {
		auto  line_it = --line_begins.lower_bound({ source_pos, -1 });
		usize line    = line_it->second;
		usize col     = source_pos - line_it->first + 1;
		return { line, col };
	}

	base::RawView TokenSource::getCharRange(usize begin_char, usize end_char) {
		//@TODO: add checks
		base::RawArray begin = content->view().getBegin() + decoded->at(begin_char).index;
		usize          size  = decoded->at(end_char).index - decoded->at(begin_char).index;
		return { begin, size };
	}

	/**
	 * @brief Splits a range of characters [begin_char, end_char) into lines.
	 */
	std::vector<std::pair<usize, base::RawView>> TokenSource::viewSplitRange(
		usize begin_char, usize end_char
	) {
		usize                                        begin_line = getLineColumn(begin_char).first;
		usize                                        end_line   = getLineColumn(end_char - 1).first;
		std::vector<std::pair<usize, base::RawView>> res;

		if (end_char == 0) return res;

		for (usize line = begin_line; line <= end_line; line++) {
			auto view = getCharRange(
				std::max(begin_char, getLine(line).first), std::min(end_char, getLine(line).second)
			);

			if (begin_line != end_line && view.size() == 0
			    && getLine(line).first != getLine(line).second) {
				continue;
			}

			res.emplace_back(line, view);
		}
		return res;
	}

	void TokenSource::runLexer(const lexer::LexOptions& options) {
		if (int_log.hasErrors()) return;
		lexer::Lexer lexer{ Ref<TokenSource>(this), options };
		token_data.emplace(lexer.tokenize());
	}

	const base::SharedView TokenSource::getContent() const { return content.value(); }

	const lexer::CharArray& TokenSource::getChars() const {
		if (!decoded) CORE_PANIC("Tried to access nonexistant Character data.");
		return decoded.value();
	}

	const lexer::TokenData& TokenSource::getTokenData() const {
		// @TODO: Maybe use lexer to create it.
		if (!token_data) CORE_PANIC("Tried to access nonexistant token data.");
		return token_data.value();
	}

	fs::File TokenSource::getFile() const { return location->getSourceFile(); }

	CRef<dia::Location> TokenSource::getLocation() const { return location.ref(); }
}
