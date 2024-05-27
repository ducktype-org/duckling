#pragma once

#include "diagnostic/message.hpp"
#include "diagnostic/source_position.hpp"
#include "token_stream.hpp"

#include <diagnostic/logger.hpp>

namespace tpc {

	/**
	 * @brief Implements higher level token stream interactions
	 */
	class ParserState {
		std::vector<TokenStream> stream_stack;  ///< Internal storage of recursive strings

	public:
		/**
		 * @brief provides mutable access to the current stream
		 */
		TokenStream& tokens();
		/**
		 * @brief provides immutable access to the current stream
		 */
		[[nodiscard]]
		const TokenStream& ctokens() const;

		dia::Logger& err;  ///< Stores parsing errors

		ParserState(TokenStream&& tokens, dia::Logger& err): err(err) {
			stream_stack.emplace_back(std::move(tokens));
		}

		/**
		 * @return true If no tokens left in current stream
		 * @return false If tokens left in current stream
		 */
		[[nodiscard]]
		bool empty() const;
		/**
		 * @return true If tokens left in current stream
		 * @return false If no tokens left in current stream
		 */
		[[nodiscard]]
		bool notEmpty() const;

		/**
		 * @return true If token on the relative position is an end of file.
		 * @return true If token on the relative position is not an end of file.
		 */
		[[nodiscard]]
		bool isEOF(i64 fwd = 0) const;

		/**
		 * @brief Creates a new stream from the current token in current stream and makes it the
		 * current stream
		 */
		void goDown();
		/**
		 * @brief deletes current stream and makes last stream the current stream
		 */
		void goUp();
		/**
		 * @brief deletes current stream and makes last stream the current stream then skips one
		 * token(the recursive token that was the source of the deleted stream)
		 */
		void goUpAndSkip();

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void fail(usize rel_pos, const std::string& message) {
			err.failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void fail(base::unique_ptr<dia::Message> message) { err.log(std::move(message)); }

		/**
		 * @brief Get position relative to the current token.
		 */
		dia::SourcePosition getPosition(i64 fwd = 0) { return ctokens().peek(fwd).getPosition(); }

		/**
		 * @brief Get position range relative to the current token.
		 */
		dia::SourcePosition getPosition(i64 fwd_from, i64 fwd_to);

		/**
		 * @brief Skips current token if it's equal to @p key.
		 *
		 * @return If the token was skipped.
		 */
		bool tryEat(Keyword key) {
			if (tokens().is(key)) {
				tokens().next();
				return true;
			}
			return false;
		}

		/**
		 * @brief Skips current token if it's equal to @p spec.
		 *
		 * @return If the token was skipped.
		 */
		bool tryEat(Special spec) {
			if (tokens().is(spec)) {
				tokens().next();
				return true;
			}
			return false;
		}

		/**
		 * @brief Skips current token if it's equal to @p op.
		 *
		 * @return If the token was skipped.
		 */
		bool tryEat(Operator op) {
			if (tokens().is(op)) {
				tokens().next();
				return true;
			}
			return false;
		}
	};

}
