#pragma once

#include "token_stream.hpp"
#include "parser_ref.hpp"

#include <diagnostic/error_state.hpp>

namespace tpc {

	/**
	 * @brief Implements higher level token stream interactions
	 */
	class ParserState {
		std::vector<TokenStream> stream_stack; ///< Internal storage of recursive strings

	public:
		/**
		 * @brief provides mutable access to the current stream
		 */
		TokenStream&       tokens();
		/**
		 * @brief provides immutable access to the current stream
		 */
		const TokenStream& ctokens() const;

		dia::ErrorState err; ///< Stores parsing errors

		ParserState(TokenStream&& tokens, dia::ErrorState&& err): err(std::move(err)) {
			stream_stack.emplace_back(std::move(tokens));
		}

		/**
		 * @return true If no tokens left in current stream
		 * @return false If tokens left in current stream
		 */
		bool empty() const;
		/**
		 * @return true If tokens left in current stream
		 * @return false If no tokens left in current stream
		 */
		bool notEmpty() const;

		/**
		 * @brief Creates a new stream from the current token in current stream and makes it the current stream
		 */
		void goDown();
		/**
		 * @brief deletes current stream and makes last stream the current stream
		 */
		void goUp();
		/**
		 * @brief deletes current stream and makes last stream the current stream then skips one token(the recursive token that was the source of the deleted stream)
		 */
		void goUpAndSkip();

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void fail(usize rel_pos, std::string message) {
			err.failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

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
