#pragma once

#include "access.hpp"
#include "elements/hierarchy/declarations/top_level.hpp"
#include "elements/includes/basic.hpp"  // IWYU pragma: keep
#include "lang_parser_context.hpp"
#include "pst_state_forward.hpp"

#include <time_stats/time_stats.hpp>

#include <diagnostic/logger.hpp>
#include <diagnostic/stable_position.hpp>
#include <token_source/source.hpp>

namespace pst {
	// Used to not include full state definition
	namespace internal {
		Box<LangParserState> makeState(
			tpc::TokenStream&&, Box<LangParserContext>&&, Ref<dia::Logger> int_logger
		);
		void finalizeParsing(Ref<LangParserState>);
	}

	/**
	 * @brief Type of file parser, used for parsing context defaults
	 *
	 * Program - Top level is unordered
	 * Script - Top level is ordered
	 */
	enum class PSTType {
		Program,
		Script,
	};

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
	class PST final {
	public:
		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		constexpr static bool PARSE_ABLE_EMPTY
			= tpc::ParseAbleElement<Element, Parser, LangParserState>;

		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		template<typename... Args>
		constexpr static bool PARSE_ABLE
			= tpc::ParseAbleElement<Element, Parser, LangParserState, Args...>;

		/**
		 * @brief The context passed to parsing.
		 *
		 * For most cases PSTType is enough as it covers the default options
		 * When passing of custom context is needed the box with context option can be used.
		 *
		 * When making an expanded tree only the passed box with context is accepted.
		 */
		using PSTContext = std::variant<PSTType, Box<LangParserContext>>;


	private:
		/****************\
		|    PST DATA    |
		\****************/

		/**
		 * Token source backing this PST (tokenized file or virtual input).
		 */
		Box<tokenizer::TokenSource> file;

		/**
		 * Root element access wrapper for the parsed element tree.
		 */
		AccessInternalAnonymous<Element> element;

		/**
		 * Contextual component path/hash of this PST for hierarchical naming.
		 */
		hashing::ComponentHash hash_ctx_info;

		/***********************\
		|    PRIVATE METHODS    |
		\***********************/

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<typename... Args>
		void parse(Box<LangParserContext>&& parsing_ctx, Args&&... args)
			requires PARSE_ABLE<Args...> {
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
                std::move(parsing_ctx),
                file->getIntLogger()
            );
			element = Parser::parse(*state_box, std::forward<Args>(args)...);
			internal::finalizeParsing(state_box.refMut());

			// Note: hash calculation should work even on errors in PST.
			// We let it be calculated to don't worry about hash being unavailable during the
			// compiler initialization phase, but we generally stop the compilation when there are
			// errors anyway. if it breaks consider wrapping the lines in `if (not hasErrors())` and
			// handling it differently.
			calcElementPathHash();
			calcHashes();

			// @TODO: #2404 prevent putInPSTHashHashMap before the generated PST is signed
			putInPSTHashHashMap();
		}

		static Box<LangParserContext> makeParserContext(PSTContext&& pst_ctx) {
			if (std::holds_alternative<PSTType>(pst_ctx)) {
				switch (std::get<PSTType>(pst_ctx)) {
				case PSTType::Program:
					return LangParserContext::programBaseContext();
				case PSTType::Script:
					return LangParserContext::scriptBaseContext();
				default:
					CORE_PANIC("PST type not implemented");
				}
			} else if (std::holds_alternative<Box<LangParserContext>>(pst_ctx)) {
				return std::get<Box<LangParserContext>>(std::move(pst_ctx));
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		template<typename... Args>
		explicit PST(
			std::string_view         content,
			Box<LangParserContext>&& parsing_ctx,
			hashing::ComponentHash   hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Args...>:
			  file(tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(content))),
			  hash_ctx_info(std::move(hash_ctx)) {
			if (!file->tokenize()) return;
			parse(std::move(parsing_ctx), std::forward<Args>(args)...);
		}

		/**
		 * @brief Construct a new Pst from expanded text
		 */
		template<typename... Args>
		explicit PST(
			dia::StablePosition    pos,
			std::string_view       content,
			Box<LangParserContext> parsing_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Args...>
			  : file(tokenizer::makeTokenSource(pos, content)), hash_ctx_info(std::move(hash_ctx)) {
			if (!file->tokenize()) return;
			parse(std::move(parsing_ctx), std::forward<Args>(args)...);
		}

		/**
		 * @TODO: #3110 this constructor is totally hacked, change it.
		 * We should somehow be able to share token_source between the original and cloned PST.
		 *
		 * Also: add clone dummy parameter here, to make it more explicit.
		 */
		explicit PST(
			Box<Element>                cloned_element,
			Box<tokenizer::TokenSource> token_source,
			hashing::ComponentHash      hash_ctx = {}
		):
			  file(std::move(token_source)),
			  element(AccessInternalAnonymous<Element>(std::move(cloned_element))),
			  hash_ctx_info(std::move(hash_ctx)) {
			calcElementPathHash();
			calcHashes();
			putInPSTHashHashMap();
		}

		/**
		 * @brief Performs the element path calculation for all of the elements of the tree.
		 */
		void calcElementPathHash() {
			if (auto ref = element.internalMut()) ref->calcElementPathHash(hash_ctx_info);
		}

		/**
		 * @brief Performs the hash calculation for all of the elements of the tree.
		 */
		void calcHashes() {
			if (auto ref = element.internalMut()) ref->calcHashRecursive();
		}

		void putInPSTHashHashMap() {
			if (auto ref = element.internalMut()) ref->putInPSTHashHashMapRecursive();
		}

		/**
		 * @brief Calculates the total signature (Hash of the whole pst) and signs all of the
		 * elements with it (Adds it to their hash).
		 */
		void signGenerated() {
			if (auto ref = element.internalMut()) {
				HashAlg partial_hash{};
				ref->calcSignature(partial_hash);
				auto hash = partial_hash.finalize();
				ref->signGenerated(hash);
			}
		}

	public:
		/**********************\
		|    PUBLIC METHODS    |
		\**********************/

		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		PST(Box<tokenizer::TokenSource> file,
		    PSTContext&&                pst_ctx,
		    hashing::ComponentHash      hash_ctx = {})

		requires PARSE_ABLE_EMPTY: file(std::move(file)), hash_ctx_info(std::move(hash_ctx)) {
			if (getLogger()->bad()) return;
			parse(makeParserContext(std::move(pst_ctx)));
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PST(const fs::File& path, PSTContext&& pst_ctx, hashing::ComponentHash hash_ctx = {})

		requires PARSE_ABLE_EMPTY:
			  file(tokenizer::makeTokenSource(path)),
			  hash_ctx_info(std::move(hash_ctx)) {
			if (!file->tokenize()) return;
			parse(makeParserContext(std::move(pst_ctx)));
		}

		static PST fromContents(
			std::string_view contents, PSTContext&& pst_ctx, hashing::ComponentHash hash_ctx = {}
		) requires PARSE_ABLE_EMPTY {
			return PST(contents, makeParserContext(std::move(pst_ctx)), std::move(hash_ctx));
		}

		template<typename... Args>
		static PST fromContentsWithArgs(
			std::string_view       contents,
			PSTContext&&           pst_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Args...> {
			return PST(
				contents,
				makeParserContext(std::move(pst_ctx)),
				std::move(hash_ctx),
				std::forward<Args>(args)...
			);
		}

		/** @brief Create a PST from an expanded (macro) text, with correct query dependency
		 * tracking via unlock(ctx). */
		static PST fromExpand(
			dia::StablePosition      pos,
			std::string_view         contents,
			Box<LangParserContext>&& parsing_ctx,
			hashing::ComponentHash   hash_ctx = {}
		) {
			return fromExpandWithArgs(pos, contents, std::move(parsing_ctx), hash_ctx);
		}

		template<typename... Args>
		static PST fromExpandWithArgs(
			dia::StablePosition    pos,
			std::string_view       contents,
			Box<LangParserContext> parsing_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Args...> {
			auto out = PST(
				pos, contents, std::move(parsing_ctx), std::move(hash_ctx), std::forward<Args>(args)...
			);
			// @TODO: #2404 Both signing and hashing should be performed in the parse function, it
			// should receive some kind of "options/PSTContext" struct simillar to the
			// LangParserContext that will define whether the PST is generated, etc.

			out.signGenerated();

			// we call it again after signing, because signing changes the hash:
			out.putInPSTHashHashMap();
			return out;
		}

		/**
		 * @brief Create a PST from a cloned element.
		 * @TODO: #3110 this constructor is totally hacked, change it.
		 * We should somehow be able to share token_source between the original and cloned PST.
		 */
		static PST fromClone(
			Box<Element>                cloned_element,
			Box<tokenizer::TokenSource> token_source,
			hashing::ComponentHash      hash_ctx
		) {
			return PST(std::move(cloned_element), std::move(token_source), std::move(hash_ctx));
		}

		[[nodiscard]]
		CRef<dia::Logger> getLogger() const {
			return file->getIntLogger();
		}

		[[nodiscard]]
		Ref<dia::Logger> getLoggerMut() {
			return file->getIntLogger();
		}

		[[nodiscard]] bool hasErrors() const { return file->getIntLogger()->hasErrors(); }

		[[nodiscard]]
		CRef<tokenizer::TokenSource> getFile() const {
			return file.ref();
		}

		[[nodiscard]]
		AccessLocked<Element> getRootElement() const {
			return element.give();
		}

		PST(PST&& other) noexcept:
			  file(std::move(other.file)),
			  element(std::move(other.element)),
			  hash_ctx_info(std::move(other.hash_ctx_info)) {}

		/**
		 * @TODO: #2397 Additional root data should just be passed during construction.
		 */
		void setAdditionalRootData(AdditionalRootData data) {
			CORE_ASSERT(
				element.internalMut().toOpt().has_value(),
				"Attempted to set additional root data on PST with null root element"
			);
			this->element.internalMut()->setAdditionalRootData(std::move(data));
		}

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}
