#pragma once

#include "base/raw_view.hpp"
#include "lexer/char.hpp"
#include <base/borrow_pointer.hpp>
#include <base/unique_pointer.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <filesystem/encoding.hpp>
#include <lexer/token.hpp>
#include <lexer/decode.hpp>
#include <lexer/lexer.hpp>

#include <set>

namespace tokenizer {
	class TokenFile;
	using BorrowFile = base::borrow_ptr<TokenFile>;
	using OwnFile    = base::unique_ptr<TokenFile>;

	/**
	 * @brief Class managing source file data access and token metadata
	 *
	 * @note For now it's very minimal
	 */
	class TokenFile {
	private:
		fs::FilePath                     path;
		dia::Logger                      log;
		base::Optional<fs::FileContent>  content;
		base::Optional<lexer::CharArray> decoded;
		base::Optional<lexer::TokenData> token_data;

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

		BorrowFile self;

		template<class... Ts>
		friend base::unique_ptr<TokenFile> makeTokenFile(Ts&&... args);

	public:
		explicit TokenFile(const fs::FilePath&);

		TokenFile(const TokenFile&) = delete;
		TokenFile()                 = delete;

		TokenFile(TokenFile&&) = default;

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

		std::vector<std::pair<usize, usize>>& getLines() { return lines; }

		fs::FileContent   getContent();
		lexer::CharArray& getChars();
		lexer::TokenData& getTokenData();
		dia::Logger&      getLogger();
		fs::FilePath      getPath();

		template<fs::Encoding encoding = fs::Encoding::UTF8>
		void decode() {
			decoded = lexer::decode<encoding>(content->view(), log);
		}

		void countLines();

		void runLexer();

		/**
		 * @brief Run the whole lexer.
		 */
		template<fs::Encoding encoding = fs::Encoding::UTF8>
		void tokenize() {
			lexer::init();
			decode<encoding>();
			countLines();
			runLexer();
		}
	};

	template<class... Ts>
	base::unique_ptr<TokenFile> makeTokenFile(Ts&&... args) {
		auto ptr  = base::make_unique<TokenFile>(std::forward<Ts...>(args...));
		ptr->self = ptr.borrow_mut();
		return ptr;
	}
}
