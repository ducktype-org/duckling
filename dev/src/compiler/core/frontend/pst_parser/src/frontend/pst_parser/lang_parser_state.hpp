#pragma once

#include "lang_parser_element.hpp"
#include "pst_automatic.hpp"
#include "pst_state_forward.hpp"  // IWYU pragma: keep

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <logger/logger.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>

#include <utility>

namespace pst {

	/**
	 * @brief State used for parsing Duckling to PST
	 */
	class LangParserState final: public tpc::ParserState {
		std::vector<ImportType> imports;

		bool skip_till_fallback = false;  ///< Tells whether parser is currently skipping the
		                                  ///< parsing steps to get back to fallback.
		u64 skipped_entries_depth = 0;    ///< Keeps balance of skipped entries to new fallbacks.
		                                  ///< Original fallback is only reached when it is 0.
		bool finalized = false;

		void checkAllParsed();

	public:
		LangParserState(
			tpc::TokenStream&& tokens, Ref<dia::Logger> err, Ref<dia_int::Logger> int_err
		):
			  tpc::ParserState(std::move(tokens), err, int_err) {}

		/**
		 * @brief Informs whether new errors and some parsing should be skipped till fallback is
		 * reached.
		 */
		[[nodiscard]]
		bool isSkipping() const;

		/**
		 * @brief Informs whether the state is finalized
		 */
		[[nodiscard]]
		bool isFinalized() const;

		/**
		 * @brief Adds import to the list of imports.
		 */
		void addImport(const CRef<pst::Import>& import);

		/**
		 * @brief Extracts imports from state.
		 *
		 * @note Leaves State in an `illegal` state.
		 */
		[[nodiscard]]
		auto extractState() && -> std::vector<ImportType> {
			CORE_ASSERT(isFinalized(), "Parsing was not finalized before extracting imports");
			return std::move(imports);
		}

		/**
		 * @brief deletes current stream and makes last stream the current stream. Resets error
		 * bit (additional errors are no longer ignored). This will produce an error if the whole
		 * sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void goUp() override;
		/**
		 * @brief deletes current stream and makes last stream the current stream then skips one
		 * token (the recursive token that was the source of the deleted stream). Resets error
		 * bit (additional errors are no longer ignored). This will produce an error if the whole
		 * sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void goUpAndSkip() override;

		/**
		 * @brief Creates a new sub-stream of given length starting in the current token.
		 */
		void setFallback(u64 length);

		/**
		 * @brief Goes back from the fallback sub-stream to the fallback position. Resets error
		 * bit (additional errors are no longer ignored). This will produce an error if the whole
		 * sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void exitFallback();

		/**
		 * @brief Do final checks that everything is parsed.
		 */
		void finalize();

		/**
		 * @brief Adds to the balance of skipped_entries
		 */
		void skipEntry() {
			CORE_ASSERT(
				skip_till_fallback,
				"Skipped entries depth can only be counted during skipping till fallback"
			);
			CORE_ASSERT(
				skipped_entries_depth, "Illegal state, if the depth is 0 then we found the fallback"
			);
			skipped_entries_depth++;
		}

		/**
		 * @brief Adds to the balance of skipped_entries
		 */
		bool removeEntry() {
			CORE_ASSERT(
				skip_till_fallback, "Entries can only be counted during skipping till fallback."
			);
			CORE_ASSERT(
				skipped_entries_depth, "Illegal state, if the depth is 0 then we found the fallback."
			);
			skipped_entries_depth--;
			return skipped_entries_depth == 0;
		}

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void fail(i64 rel_pos, const std::string& message) override {
			if (isSkipping()) {
				CORE_DEV_LOG(
					Parser,
					"Skipped parsing error at pos(",
					ctokens().peek(rel_pos).getPosition().getStartLineColumn(),
					"): ",
					message,
					"\n\n"
				);
				return;
			}
			err->failAndLog(ctokens().peek(rel_pos).getPosition(), message);
			skip_till_fallback    = true;
			skipped_entries_depth = 1;
		}

		/**
		 * @brief Logs an error relatively to the current token.
		 */
		void log(Box<dia::Message> message) override {
			if (isSkipping()) {
				CORE_DEV_LOG(
					Parser,
					"Skipped parsing message at pos(",
					message->getSourcePosition().getStartLineColumn(),
					"): ",
					message->toString(true),
					"\n\n"
				);
				return;
			}
			if (message->getSeverity() == dia::Message::Severity::Error) {
				skip_till_fallback    = true;
				skipped_entries_depth = 1;
			}
			err->log(std::move(message));
		}

		void logInt(Box<dia_int::MessageBase> message) override {
			if (isSkipping()) {
				CORE_DEV_LOG(
					Parser,
					"Skipped parsing message `",
					message->debugString(),
					"`"
				);
				return;
			}
			if (message->isError()) {
				skip_till_fallback    = true;
				skipped_entries_depth = 1;
			}
			int_err->log(std::move(message));
		}

		/**
		 * @brief Gives access to automatic parsing tools.
		 */
		template<std::derived_from<LangElement> El>
		pst::PSTAutomatic<LangParserState> parse(Box<El>& el) {
			return { *this, el.refMut() };
		}

		/**
		 * @brief Gives access to automatic parsing tools.
		 */
		template<std::derived_from<LangElement> El>
		pst::PSTAutomatic<LangParserState> parse(Ref<El> el) {
			return { *this, el };
		}
	};

}
