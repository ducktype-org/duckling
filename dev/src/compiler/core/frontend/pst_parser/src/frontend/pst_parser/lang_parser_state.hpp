// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lang_parser_context.hpp"
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
		bool skip_till_fallback = false;  ///< Tells whether parser is currently skipping the
		                                  ///< parsing steps to get back to fallback.
		u64 skipped_entries_depth = 0;    ///< Keeps balance of skipped entries to new fallbacks.
		                                  ///< Original fallback is only reached when it is 0.
		bool finalized = false;

		void checkAllParsed();

		void copyOwnContext();

	private:
		template<class T>
		friend class pst::PSTAutomatic;
		friend void fallbackLen(LangParserState& state, u64 length);
		friend void exitFallback(LangParserState& state);
		friend void setSoftFallback(LangParserState& state, TokenStreamCondition fun);
		friend void exitSoftFallback(LangParserState& state);

		/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
		| These are methods that should only be used by automatic             |
		\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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
		 * @brief Sets a soft fallback that tries to find a sensible end using the condition in case
		 * of error.
		 */
		void setSoftFallback(std::function<TokenStreamCondition>);

		/**
		 * @brief Exits a soft fallback that tries to find a sensible end using the condition in
		 * case of error.
		 */
		void exitSoftFallback();

		/**
		 * @brief Goes back from the fallback sub-stream to the fallback position. Resets error
		 * bit (additional errors are no longer ignored). This will produce an error if the whole
		 * sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void exitFallback();

	public:
		LangParserState(
			tpc::TokenStream&& tokens, Box<LangParserContext>&& ctx, Ref<dia::Logger> int_err
		):
			  tpc::ParserState(std::move(tokens), std::move(ctx), int_err) {}

		/**
		 * @brief Informs whether new errors occurred and some parsing should be skipped till
		 * fallback is reached.
		 */
		[[nodiscard]]
		bool isSkipping() const;

		/**
		 * @brief Informs whether the state is finalized
		 */
		[[nodiscard]]
		bool isFinalized() const;

		/**
		 * @brief Do final checks that everything is parsed.
		 */
		void finalize();

		[[nodiscard]]
		CRef<LangParserContext> getContext() const;

		void setContextClassName(base::StrID);
		void setContextBlockOrdering(BlockOrderType);
		void setContextStmt(StmtContext);

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
		 * @brief Logs an error.
		 */
		void logInt(Box<dia::MessageBase> message) override {
			if (isSkipping()) {
				CORE_DEV_LOG(Parser, "Skipped parsing message `", message->debugString(), "`");
				return;
			}
			if (message->isError()) {
				skip_till_fallback    = true;
				skipped_entries_depth = 1;
			}
			int_err->log(std::move(message));
		}

		/**
		 * @brief Logs an error that doesn't require skipping to a fallback.
		 *
		 * @note This is for very specific usecases where behaviour is reliable.
		 * Care needs to be taken so that each element has all the data needed for hashing.
		 */
		void logSafeError(Box<dia::MessageBase> message) {
			if (isSkipping()) {
				CORE_DEV_LOG(Parser, "Skipped parsing message `", message->debugString(), "`");
				return;
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
