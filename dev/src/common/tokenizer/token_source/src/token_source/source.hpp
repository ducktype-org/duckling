#pragma once

#include <diagnostic_interactive/logger.hpp>

#include <base/misc/shared_view.hpp>

#include <diagnostic/location.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/encoding.hpp>
#include <filesystem/file.hpp>
#include <lexer/char.hpp>
#include <lexer/token.hpp>
#include <token_source/forward.hpp>  // IWYU pragma: keep

#include <set>

/**
 * @TODO: Make lexer arguments explicit
 * @TODO: Add better comments for builder-like things (Lexer ...)
 */
namespace tokenizer {
	/**
	 * @brief Class managing source file data access and token metadata
	 *
	 * In normal usage it is created using makeTokenSource, then built using the tokenize() method
	 * which: decodes, splits into lines then lexes. After that the data is ready to be used.
	 *
	 */
	class TokenSource final {
	private:
		dia::Logger                            log;
		dia_int::Logger                        int_log;
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

		/**
		 * Decode the content into a character array.
		 *
		 * @tparam encoding Which encoding should the function use.
		 *
		 * @return CharArray of decoded data
		 *
		 * @note We should probably stick to only decoding UTF-8 for now
		 */
		template<fs::Encoding encoding>
		lexer::CharArray internalDecode();

		template<>
		lexer::CharArray internalDecode<fs::UsAscii>();

		template<>
		lexer::CharArray internalDecode<fs::UTF8>();

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

		Ref<dia_int::Logger> getIntLogger() { return &int_log; }

		[[nodiscard]]
		fs::File getFile() const;

		std::vector<std::pair<usize, usize>>& getLines() { return lines; }

		template<fs::Encoding encoding = fs::Encoding::UTF8>
		void decode() {
			decoded.emplace(internalDecode<encoding>());
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
			if (int_log.hasErrors()) return false;
			countLines();
			runLexer();
			return not int_log.hasErrors();
		}
	};

	template<class... Ts>
	Box<TokenSource> makeTokenSource(Ts&&... args) {
		return Box<TokenSource>::fromPointer(new TokenSource(std::forward<Ts>(args)...));
	}
}
