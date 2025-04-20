#pragma once

#include "forward.hpp"

#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>
#include <filesystem/file.hpp>
#include <lexer/char.hpp>
#include <lexer/decode.hpp>
#include <lexer/lexer.hpp>
#include <lexer/token.hpp>

#include <base/raw_view.hpp>

#include <set>
#include <span>

namespace tokenizer {
	/**
	 * @brief Class managing source file data access and token metadata
	 *
	 * @note For now it's very minimal and doesn't check proper usage.
	 */
	class TokenFile {
	private:
		fs::FilePath                           path;
		dia::Logger                            log;
		base::Optional<const fs::FileContent>  content;
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

		explicit TokenFile(const fs::FilePath&);

		template<class T, class... Ts>
		friend base::Box<T> base::makeBox(Ts&&... args);

	public:
		TokenFile(const TokenFile&) = delete;
		TokenFile()                 = delete;

		TokenFile(TokenFile&&) = delete;

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
		const fs::FileContent getContent() const;
		[[nodiscard]]
		const lexer::CharArray& getChars() const;
		[[nodiscard]]
		const lexer::TokenData& getTokenData() const;
		dia::Logger&            getLogger();
		fs::FilePath            getPath();

		std::vector<std::pair<usize, usize>>& getLines() { return lines; }

		template<fs::Encoding encoding = fs::Encoding::UTF8>
		void decode() {
			decoded.emplace(lexer::decode<encoding>(Ref(this), log));
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
	Box<TokenFile> makeTokenFile(Ts&&... args) {
		return makeBox<TokenFile>(std::forward<Ts>(args)...);
	}
}
