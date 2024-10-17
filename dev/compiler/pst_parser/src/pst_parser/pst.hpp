#pragma once

#include "elements/elements.hpp"  // toplevel only, @TODO: change it to something better

#include <token_file/file.hpp>
#include "lang_parser_state.hpp"
#include "parser.hpp"

namespace pst {
	/**
	 * @brief PST generation class. Parses on construction if possible.
	 *
	 * @note The Element is only required to be derived from LangElement and not necessarily
	 * parsable to allow to manage already parsed generic PST<LangElement>.
	 *
	 * @tparam Element Root Element to parse.
	 */
	template<std::derived_from<LangElement> Element = TopLevel>
	class PST {
	public:
		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		template<typename... Args>
		constexpr static bool ParseAble = tpc::ParseAbleElement<LangElement, Element, LangParserState, Args...>;

	private:
		tokenizer::OwnFile      file;
		ParserRef<LangElement>      element;
		std::vector<ImportType> imports;

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<typename... Args>
		void parse(Args&&... args) requires ParseAble<Args...> {
			const lexer::TokenData& token_data = file->getTokenData();
			LangParserState         state(
                tpc::TokenStream(
                    token_data.tokens,
                    token_data.bof_sentinel,
                    token_data.eof_sentinel,
                    0,
                    token_data.tokens.size()
                ),
                file->getLogger()
            );
			element = Element::parse(state, std::forward<Args>(args)...);
			imports = std::move(state).extractState();
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		template<typename... Args>
		explicit PST(std::string_view content, Args&&... args) requires ParseAble<Args...>
			  : file(tokenizer::makeTokenFile(fs::FilePath::createTempFile(content))) {
			pst::init();
			if (!file->tokenize()) return;
			parse(std::forward<Args>(args)...);
		}

	public:
		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		PST(tokenizer::OwnFile&& file) requires ParseAble<>: file(std::move(file)) {
			pst::init();
			if (getLogger().bad()) return;
			parse();
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PST(const fs::FilePath& path) requires ParseAble<>: file(tokenizer::makeTokenFile(path)) {
			pst::init();
			if (!file->tokenize()) return;
			parse();
		}

		static PST fromContents(std::string_view contents) requires ParseAble<> {
			return PST(contents);
		}

		template<typename... Args>
		static PST fromContentsWithContext(std::string_view contents, Args&&... args)
			requires ParseAble<Args...> {
			return PST(contents, std::forward<Args>(args)...);
		}

		[[nodiscard]]
		const std::vector<ParserCBorrowRef<Import>>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		const dia::Logger& getLogger() const {
			return file->getLogger();
		}

		[[nodiscard]]
		tokenizer::BorrowFile getFile() const {
			return file.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<LangElement> getRootElement() const {
			return element.borrow();
		}

		PST(PST&& other) noexcept:
			  file(std::move(other.file)),
			  element(std::move(other.element)),
			  imports(std::move(other.imports)) {}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}
