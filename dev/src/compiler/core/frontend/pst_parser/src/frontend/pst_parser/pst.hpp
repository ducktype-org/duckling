#pragma once

#include "access.hpp"
#include "elements/hierarchy/declarations/top_level.hpp"
#include "elements/includes/basic.hpp"  // IWYU pragma: keep
#include "lang_parser_context.hpp"
#include "pst_state_forward.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <time_stats/time_stats.hpp>

#include <token_source/source.hpp>

#include <any>

namespace pst {
	// Used to not include full state definition
	namespace internal {
		Box<LangParserState> makeState(
			tpc::TokenStream&&, Box<LangParserContext>&&, Ref<dia_int::Logger> int_logger
		);
		std::vector<ImportType> extractState(Box<LangParserState>);

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
	class PST {
	public:
		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		template<typename... Args>
		constexpr static bool ParseAble
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
		 * Import entries collected during parsing.
		 */
		std::vector<ImportType> imports;

		/**
		 * Contextual component path/hash of this PST for hierarchical naming.
		 */
		hashing::ComponentHash hash_ctx_info;

		// /**
		//  * Additional data, that can be used for storing some extra information related to the PST,
		//  * in a way that does not require to include half ot the other compiler (PR TODO)
		//  * We only set it when PST is created in the full compilation context
		//  */
		// base::Optional<AdditionalData> additional_data;


		/***********************\
		|    PRIVATE METHODS    |
		\***********************/

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<typename... Args>
		void parse(Box<LangParserContext>&& parsing_ctx, Args&&... args) requires ParseAble<Args...>
		{
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
			imports = internal::extractState(std::move(state_box));


			// Note: hash calculation should work even on errors in PST.
			// We let it be calculated to don't worry about hash beeing unavailable during the
			// compiler initialization phase, but we generally stop the compilation when there are
			// errors anyway. if it breaks consider wrapping the lines in `if (not hasErrors())` and
			// handling it differently.
			calcElementPathHash();
			calcHashes();
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
		) requires ParseAble<Args...>:
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
			dia::SourcePosition    pos,
			std::string_view       content,
			Box<LangParserContext> parsing_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires ParseAble<Args...>
			  : file(tokenizer::makeTokenSource(pos, content)), hash_ctx_info(std::move(hash_ctx)) {
			if (!file->tokenize()) return;
			parse(std::move(parsing_ctx), std::forward<Args>(args)...);
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

		requires ParseAble<>: file(std::move(file)), hash_ctx_info(std::move(hash_ctx)) {
			if (getLogger()->bad()) return;
			parse(makeParserContext(std::move(pst_ctx)));
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		PST(const fs::File& path, PSTContext&& pst_ctx, hashing::ComponentHash hash_ctx = {})

		requires ParseAble<>:
			  file(tokenizer::makeTokenSource(path)),
			  hash_ctx_info(std::move(hash_ctx)) {
			if (!file->tokenize()) return;
			parse(makeParserContext(std::move(pst_ctx)));
		}

		static PST fromContents(
			std::string_view contents, PSTContext&& pst_ctx, hashing::ComponentHash hash_ctx = {}
		) requires ParseAble<> {
			return PST(contents, makeParserContext(std::move(pst_ctx)), std::move(hash_ctx));
		}

		template<typename... Args>
		static PST fromContentsWithArgs(
			std::string_view       contents,
			PSTContext&&           pst_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires ParseAble<Args...> {
			return PST(
				contents,
				makeParserContext(std::move(pst_ctx)),
				std::move(hash_ctx),
				std::forward<Args>(args)...
			);
		}

		static PST fromExpand(
			dia::SourcePosition      pos,
			std::string_view         contents,
			Box<LangParserContext>&& parsing_ctx,
			hashing::ComponentHash   hash_ctx = {}
		) {
			return fromExpandWithArgs(pos, contents, std::move(parsing_ctx), hash_ctx);
		}

		template<typename... Args>
		static PST fromExpandWithArgs(
			dia::SourcePosition    pos,
			std::string_view       contents,
			Box<LangParserContext> parsing_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires ParseAble<Args...> {
			auto out = PST(
				pos, contents, std::move(parsing_ctx), std::move(hash_ctx), std::forward<Args>(args)...
			);
			out.signGenerated();

			// we call it again after signing, because signing changes the hash:
			out.putInPSTHashHashMap();
			return out;
		}

		[[nodiscard]]
		const std::vector<ImportType>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		const Ref<dia_int::Logger> getLogger() const {
			return file->getIntLogger();
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

		// const base::Optional<AdditionalData>& getAdditionalData() const {
		// 	return additional_data;
		// }

		void dprint(std::ostream& out) const { nullAwareDprint(element, out); }
	};
}
