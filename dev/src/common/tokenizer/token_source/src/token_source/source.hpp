#pragma once

#include "forward.hpp"

#include <base/raw_view.hpp>

#include <diagnostic/location.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>
#include <filesystem/file.hpp>
#include <lexer/char.hpp>
#include <lexer/decode.hpp>
#include <lexer/lexer.hpp>
#include <lexer/token.hpp>

#include <set>

namespace tokenizer {
	/**
	 * @brief Class managing source file data access and token metadata
	 *
	 * @note For now it's very minimal and doesn't check proper usage.
	 */
	class TokenSource final {
	private:
		dia::Logger                            log;
		base::Box<dia::Location>               location;
		base::Optional<const base::SharedView> content;
		base::Optional<const lexer::CharArray> decoded;
		base::Optional<const lexer::TokenData> token_data;

		/**
		 * @brief Stores line bounds (first character of the line, character after last character)
		 * in file.
		 */
		std::vector<std::pair<usize, usize>> lines;
		/**
		 * @brief Stores pairs (Character index, line number) that represent beginnings of lines .
		 *
		 * Used for quick mapping (character index -> line).
		 */
		std::set<std::pair<usize, usize>> line_begins;

		/**
		 * @brief Construct a new TokenSource from file.
		 */
		explicit TokenSource(const fs::File&);

		/**
		 * @brief Construct a new TokenSource as a macro with parent position.
		 */
		explicit TokenSource(dia::SourcePosition parent, std::string_view contents);

		template<class... Ts>
		friend Box<TokenSource> makeTokenSource(Ts&&... args);

	public:
		TokenSource(const TokenSource&) = delete;
		TokenSource()                   = delete;

		TokenSource(TokenSource&&) = delete;

		/**
		 * @brief Compute pair (line, column) from character index.
		 */
		[[nodiscard]]
		std::pair<usize, usize> getLineColumn(usize source_pos);

		/**
		 * @brief Get line bounds (first, last + 1).
		 */
		std::pair<usize, usize> getLine(usize line) { return lines.at(line - 1); }

		/**
		 * @brief Returns a view containing the source characters in bounds [@p begin_char,@p
		 * end_char).
		 */
		base::RawView getCharRange(usize begin_char, usize end_char);

		/**
		 * @brief Returns views of a [) range split by lines.
		 */
		std::vector<std::pair<usize, base::RawView>> viewSplitRange(usize begin_char, usize end_char);

		[[nodiscard]]
		const base::SharedView getContent() const;
		[[nodiscard]]
		const lexer::CharArray& getChars() const;
		[[nodiscard]]
		const lexer::TokenData& getTokenData() const;
		[[nodiscard]]
		CRef<dia::Location> getLocation() const;
		Ref<dia::Logger>    getLogger();
		[[nodiscard]]
		fs::File getFile() const;

		std::vector<std::pair<usize, usize>>& getLines() { return lines; }

		template<fs::Encoding encoding = fs::Encoding::UTF8>
		void decode() {
			decoded.emplace(lexer::decode<encoding>(Ref(this), &log));
		}

		void countLines();

		void runLexer();

		/**
		 * @brief Run the whole lexer.
		 * @return If tokenizing process run without errors.
		 */
		template<fs::Encoding encoding = fs::Encoding::UTF8>
		bool tokenize() {
			decode<encoding>();
			if (log.bad()) return false;
			countLines();
			runLexer();
			return log.good();
		}
	};

	template<class... Ts>
	Box<TokenSource> makeTokenSource(Ts&&... args) {
		return Box<TokenSource>::fromPointer(new TokenSource(std::forward<Ts>(args
		)...));
	}
}
