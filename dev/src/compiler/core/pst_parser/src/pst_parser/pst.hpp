#pragma once

#include "access.hpp"
#include "elements/hierarchy/declarations/top_level.hpp"
#include "elements/includes/basic.hpp"  // IWYU pragma: keep
#include "pst_state_forward.hpp"

#include <token_source/source.hpp>

namespace pst {
	// Used to not include full state definition
	namespace internal {
		Box<LangParserState>    makeState(tpc::TokenStream&&, Ref<dia::Logger> logger);
		std::vector<ImportType> extractState(Box<LangParserState>);
	}

	/**
	 * @brief PST generation class. Parses on construction if possible.
	 *
	 * @note The Element is only required to be derived from LangElement and not necessarily
	 * parsable to allow to manage already parsed generic PST<LangElement>.
	 *
	 * @tparam Element Root Element to parse.
	 */
	template<
		std::derived_from<LangElement> Element = TopLevel,
		std::derived_from<LangElement> Parser  = Element>
	class PST {
	public:
		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		template<typename... Args>
		constexpr static bool ParseAble
			= tpc::ParseAbleElement<Element, Parser, LangParserState, Args...>;

	private:
		Box<tokenizer::TokenSource>      file;
		AccessInternalAnonymous<Element> element;
		std::vector<ImportType>          imports;

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<typename... Args>
		void parse(Args&&... args) requires ParseAble<Args...> {
			const lexer::TokenData& token_data = file->getTokenData();
			auto                    state_box  = internal::makeState(
                tpc::TokenStream(
                    token_data.tokens,
                    token_data.bof_sentinel,
                    token_data.eof_sentinel,
                    0,
                    token_data.tokens.size()
                ),
                file->getLogger()
            );
			element = Parser::parse(*state_box, std::forward<Args>(args)...);
			imports = internal::extractState(std::move(state_box));
			calcElementPaths();
			calcHashes();
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		template<typename... Args>
		explicit PST(std::string_view content, Args&&... args) requires ParseAble<Args...>:
			  file(tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(content))) {
			if (!file->tokenize()) return;
			parse(std::forward<Args>(args)...);
		}

		/**
		 * @brief Construct a new Pst from expanded text
		 */
		template<typename... Args>
		explicit PST(dia::SourcePosition pos, std::string_view content, Args&&... args)
			requires ParseAble<Args...>
			  : file(tokenizer::makeTokenSource(pos, content)) {
			if (!file->tokenize()) return;
			parse(std::forward<Args>(args)...);
		}

		/**
		 * @brief Starts the element path calculation for all of the elements of the tree.
		 */
		void calcElementPaths() {
			if (auto ref = element.internalMut()) ref->calcElementPaths({});
		}

		/**
		 * @brief Starts the hash calculation for all of the elements of the tree.
		 */
		void calcHashes() {
			if (auto ref = element.internalMut()) ref->calcHashRecursive();
		}

	public:
		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		PST(Box<tokenizer::TokenSource>&& file) requires ParseAble<>: file(std::move(file)) {
			if (getLogger()->bad()) return;
			parse();
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PST(const fs::File& path) requires ParseAble<>: file(tokenizer::makeTokenSource(path)) {
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

		static PST fromExpand(dia::SourcePosition pos, std::string_view contents) {
			return PST(pos, contents);
		}

		template<typename... Args>
		static PST fromExpandWithContext(
			dia::SourcePosition pos, std::string_view contents, Args&&... args
		) requires ParseAble<Args...> {
			return PST(pos, contents, std::forward<Args>(args)...);
		}

		[[nodiscard]]
		const std::vector<ImportType>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		const Ref<dia::Logger> getLogger() const {
			return file->getLogger();
		}

		[[nodiscard]]
		Ref<tokenizer::TokenSource> getFile() const {
			return file.ref();
		}

		[[nodiscard]]
		AccessLocked<Element> getRootElement() const {
			return element.give();
		}

		PST(PST&& other) noexcept:
			  file(std::move(other.file)),
			  element(std::move(other.element)),
			  imports(std::move(other.imports)) {}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}
