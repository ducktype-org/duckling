#pragma once

#include "access.hpp"
#include "elements/hierarchy/declarations/top_level.hpp"
#include "elements/includes/basic.hpp"  // IWYU pragma: keep
#include "pst_state_forward.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <time_stats/time_stats.hpp>

#include <token_source/source.hpp>

namespace pst {
	// Used to not include full state definition
	namespace internal {
		Box<LangParserState> makeState(
			tpc::TokenStream&&, Ref<dia::Logger> logger, Ref<dia_int::Logger> int_logger
		);
		std::vector<ImportType> extractState(Box<LangParserState>);

		void finalizeParsing(Ref<LangParserState>);
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
		/** Token source backing this PST (tokenized file or virtual input). */
		Box<tokenizer::TokenSource> file;
		/** Root element access wrapper for the parsed element tree. */
		AccessInternalAnonymous<Element> element;
		/** Import entries collected during parsing. */
		std::vector<ImportType> imports;
		/** Contextual component path/hash of this PST for hierarchical naming. */
		hashing::ComponentHash context_info;

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<typename... Args>
		void parse(Args&&... args) requires ParseAble<Args...> {
			time_stats::TrackCategoryTime track_time(time_stats::TimeCategories::PSTConstruction);

			const lexer::TokenData& token_data = file->getTokenData();
			auto                    state_box  = internal::makeState(
                tpc::TokenStream(
                    token_data.tokens,
                    token_data.bof_sentinel,
                    token_data.eof_sentinel,
                    0,
                    token_data.tokens.size()
                ),
                file->getLogger(),
                file->getIntLogger()
            );
			element = Parser::parse(*state_box, std::forward<Args>(args)...);
			internal::finalizeParsing(state_box.refMut());
			bool is_good = internal::isGood(*state_box);
			imports      = internal::extractState(std::move(state_box));
			if (is_good) {
				calcElementPathHash();
				calcHashes();
			}
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		template<typename... Args>
		explicit PST(std::string_view content, hashing::ComponentHash context = {}, Args&&... args)
			requires ParseAble<Args...>:
			  file(tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(content))),
			  context_info(std::move(context)) {
			if (!file->tokenize()) return;
			parse(std::forward<Args>(args)...);
		}

		/**
		 * @brief Construct a new Pst from expanded text
		 */
		template<typename... Args>
		explicit PST(
			dia::SourcePosition    pos,
			std::string_view       content,
			hashing::ComponentHash context = {},
			Args&&... args
		) requires ParseAble<Args...>
			  : file(tokenizer::makeTokenSource(pos, content)), context_info(std::move(context)) {
			if (!file->tokenize()) return;
			parse(std::forward<Args>(args)...);
		}

		/**
		 * @brief Performs the element path calculation for all of the elements of the tree.
		 */
		void calcElementPathHash() {
			if (auto ref = element.internalMut()) ref->calcElementPathHash(context_info);
		}

		/**
		 * @brief Performs the hash calculation for all of the elements of the tree.
		 */
		void calcHashes() {
			if (auto ref = element.internalMut()) ref->calcHashRecursive();
		}

	public:
		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		PST(Box<tokenizer::TokenSource> file, hashing::ComponentHash context = {})

		requires ParseAble<>: file(std::move(file)), context_info(std::move(context)) {
			if (getLogger()->bad()) return;
			parse();
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PST(const fs::File& path, hashing::ComponentHash context = {})

		requires ParseAble<>:
			  file(tokenizer::makeTokenSource(path)),
			  context_info(std::move(context)) {
			if (!file->tokenize()) return;
			parse();
		}

		static PST fromContents(std::string_view contents, hashing::ComponentHash context = {})
			requires ParseAble<> {
			return PST(contents, std::move(context));
		}

		template<typename... Args>
		static PST fromContentsWithArgs(
			std::string_view contents, hashing::ComponentHash context = {}, Args&&... args
		) requires ParseAble<Args...> {
			return PST(contents, std::move(context), std::forward<Args>(args)...);
		}

		static PST fromExpand(
			dia::SourcePosition pos, std::string_view contents, hashing::ComponentHash context = {}
		) {
			return PST(pos, contents, std::move(context));
		}

		template<typename... Args>
		static PST fromExpandWithArgs(
			dia::SourcePosition    pos,
			std::string_view       contents,
			hashing::ComponentHash context = {},
			Args&&... args
		) requires ParseAble<Args...> {
			return PST(pos, contents, std::move(context), std::forward<Args>(args)...);
		}

		[[nodiscard]]
		const std::vector<ImportType>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		const Ref<dia::Logger> getLogger() const {
			return file->getLogger();
		}

		[[nodiscard]] bool hasErrors() const {
			return file->getLogger()->bad() || file->getIntLogger()->hasErrors();
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
			  imports(std::move(other.imports)),
			  context_info(std::move(other.context_info)) {}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}
