#pragma once

#include "base_element.hpp"

#include "common_elements.hpp"

#include "token_stream.hpp"
#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>

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

		// clang-format off
		[[nodiscard]]
		inline const Token& operator[](i64 fwd) const {
			return ctokens().peek(fwd);
		}

		// clang-format on

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
		 * @return false If token on the relative position is not an end of file.
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
		void fail(i64 rel_pos, const std::string& message) {
			err.failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void log(Box<dia::Message> message) { err.log(std::move(message)); }

		/**
		 * @brief Get position relative to the current token.
		 */
		[[nodiscard]]
		dia::SourcePosition getPosition(i64 fwd = 0) const {
			return ctokens().peek(fwd).getPosition();
		}

		/**
		 * @brief Get position range relative to the current token.
		 */
		[[nodiscard]]
		dia::SourcePosition getPosition(i64 fwd_from, i64 fwd_to) const;

		/**
		 * @brief Skips current token if is equal to @p t.
		 *
		 * @return If the token was skipped.
		 */
		template<typename T>
		bool tryEat(T t) {
			if (ctokens().peek().is(t)) {
				tokens().next();
				return true;
			}
			return false;
		}
	};
}
