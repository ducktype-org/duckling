#pragma once

#include "lang_parser_context.hpp"
#include "pst.hpp"
#include "pst_state_forward.hpp"
#include "pst_type.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/stable_position.hpp>
#include <time_stats/time_stats.hpp>

#include <token_source/source.hpp>

namespace pst {
	// Used to not include full state definition
	namespace internal {
		Box<LangParserState> makeState(
			tpc::TokenStream&&, Box<LangParserContext>&&, Ref<dia_int::Logger> int_logger
		);
		std::vector<ImportType> extractState(Box<LangParserState>);

		void finalizeParsing(Ref<LangParserState>);

		template<typename T>
		class ConstructorTemplateInference {};
	}

	/**
	 * @brief PST generation class. Parses on construction if possible.
	 *
	 * @note The Element is only required to be derived from LangElement and not necessarily
	 * parsable to allow to manage already parsed generic PST<LangElement>.
	 *
	 * @tparam Element Root Element to parse.
	 */
	template<std::derived_from<LangElement> Element = TopLevel>
	class ParsedPST final: public PST<Element> {
	public:
		/**
		 * @brief Checks if an element is pars-able using given arguments.
		 */
		template<std::derived_from<LangElement> Parser, typename... Args>
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
		 * Import entries collected during parsing.
		 */
		std::vector<ImportType> imports;

		/***********************\
		|    PRIVATE METHODS    |
		\***********************/

		/**
		 * @note Requires that the file was successfully tokenized.
		 */
		template<std::derived_from<LangElement> Parser, typename... Args>
		void parse(Box<LangParserContext>&& parsing_ctx, Args&&... args)
			requires PARSE_ABLE<Parser, Args...> {
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
			this->assignRoot(Parser::parse(*state_box, std::forward<Args>(args)...));
			internal::finalizeParsing(state_box.refMut());
			imports = internal::extractState(std::move(state_box));
		}

		template<typename Parser, typename... Args>
		void parseGenerated(Args&&... args) {
			parse<Parser>(std::forward<Args>(args)...);
			PST<Element>::finishGeneratedPST();
		}

		template<typename Parser, typename... Args>
		void parseInput(Args&&... args) {
			parse<Parser>(std::forward<Args>(args)...);
			PST<Element>::finishInputPST();
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
		 * @brief A custom makeBox with extended visibility.
		 */
		template<typename Parser, typename... Args>
		static auto makeParsedPstBox(Args&&... args) {
			return Box<ParsedPST>::fromPointer(new ParsedPST(
				internal::ConstructorTemplateInference<Parser>(), std::forward<Args>(args)...
			));
		}

		/**
		 * @brief Construct a new Pst from text content
		 */
		template<typename Parser, typename... Args>
		explicit ParsedPST(
			internal::ConstructorTemplateInference<Parser>,
			std::string_view         content,
			Box<LangParserContext>&& parsing_ctx,
			hashing::ComponentHash   hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Parser, Args...>:
			  PST<Element>(std::move(hash_ctx)),
			  file(tokenizer::makeTokenSource(fs::FileManager::createRandomVirtualFile(content))) {
			if (!file->tokenize()) return;
			parseInput<Parser>(std::move(parsing_ctx), std::forward<Args>(args)...);
		}

		/**
		 * @brief Construct a new Pst from expanded text
		 */
		template<typename Parser, typename... Args>
		explicit ParsedPST(
			internal::ConstructorTemplateInference<Parser>,
			dia_int::StablePosition pos,
			std::string_view        content,
			Box<LangParserContext>  parsing_ctx,
			hashing::ComponentHash  hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Parser, Args...>
			  : PST<Element>(std::move(hash_ctx)), file(tokenizer::makeTokenSource(pos, content)) {
			if (!file->tokenize()) return;
			parseGenerated<Parser>(std::move(parsing_ctx), std::forward<Args>(args)...);
		}

		/**
		 * @brief Construct a new Pst from tokenized file
		 */
		template<typename Parser>
		explicit ParsedPST(
			internal::ConstructorTemplateInference<Parser>,
			Box<tokenizer::TokenSource> file,
			PSTContext&&                pst_ctx,
			hashing::ComponentHash      hash_ctx = {}
		)

			requires PARSE_ABLE<Parser>
			  : PST<Element>(std::move(hash_ctx)), file(std::move(file)) {
			if (getLogger()->bad()) return;
			parseInput<Parser>(makeParserContext(std::move(pst_ctx)));
		}

		/**
		 * @brief Construct a new Pst from file path
		 */
		template<typename Parser>
		explicit ParsedPST(
			internal::ConstructorTemplateInference<Parser>,
			const fs::File&        path,
			PSTContext&&           pst_ctx,
			hashing::ComponentHash hash_ctx = {}
		)

			requires PARSE_ABLE<Parser>
			  : PST<Element>(std::move(hash_ctx)), file(tokenizer::makeTokenSource(path)) {
			if (!file->tokenize()) return;
			parseInput<Parser>(makeParserContext(std::move(pst_ctx)));
		}

	public:
		/**********************\
		|    PUBLIC METHODS    |
		\**********************/

		template<typename Parser = Element, typename... Args>
		static Box<ParsedPST> fromContents(
			std::string_view       contents,
			PSTContext&&           pst_ctx,
			hashing::ComponentHash hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Parser, Args...> {
			return makeParsedPstBox<Parser>(
				contents,
				makeParserContext(std::move(pst_ctx)),
				std::move(hash_ctx),
				std::forward<Args>(args)...
			);
		}

		template<typename Parser = Element, typename... Args>
		static Box<ParsedPST> fromExpand(
			dia_int::StablePosition pos,
			std::string_view        contents,
			Box<LangParserContext>  parsing_ctx,
			hashing::ComponentHash  hash_ctx = {},
			Args&&... args
		) requires PARSE_ABLE<Parser, Args...> {
			auto out = makeParsedPstBox<Parser>(
				pos, contents, std::move(parsing_ctx), std::move(hash_ctx), std::forward<Args>(args)...
			);
			CORE_ASSERT(out->imports.size() == 0, "Imports are not supported in expands");
			return out;
		}

		template<typename Parser = Element>
		static Box<ParsedPST> fromFile(
			Box<tokenizer::TokenSource>&& file,
			PSTContext&&                  pst_ctx,
			hashing::ComponentHash        hash_ctx = {}
		) requires PARSE_ABLE<Parser> {
			auto out
				= makeParsedPstBox<Parser>(std::move(file), std::move(pst_ctx), std::move(hash_ctx));
			return out;
		}

		template<typename Parser = Element>
		static Box<ParsedPST> fromFile(
			const fs::File& path, PSTContext&& pst_ctx, hashing::ComponentHash hash_ctx = {}
		) requires PARSE_ABLE<Parser> {
			auto out = makeParsedPstBox<Parser>(path, std::move(pst_ctx), std::move(hash_ctx));
			return out;
		}

		[[nodiscard]]
		const std::vector<ImportType>& getImports() const {
			return imports;
		}

		[[nodiscard]]
		CRef<dia_int::Logger> getLogger() const {
			return file->getIntLogger();
		}

		[[nodiscard]]
		Ref<dia_int::Logger> getLoggerMut() {
			return file->getIntLogger();
		}

		[[nodiscard]] bool hasErrors() const { return file->getIntLogger()->hasErrors(); }

		[[nodiscard]]
		Ref<tokenizer::TokenSource> getFile() const {
			return file.ref();
		}
	};
}
