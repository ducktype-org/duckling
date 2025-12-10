#pragma once

#include "token_stream.hpp"

#include <diagnostic_interactive/logger_fwd.hpp>

#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>

namespace tpc {

	/**
	 * @brief Implements higher level token stream interactions
	 */
	class ParserState {

		/**
		 * @brief Types of substream:
		 * - Recursive - Comes from the token structure.
		 * - NonRecursive - Manually set fallback.
		 */
		enum SubStreamType {
			Recursive,
			NonRecursive
		};

		std::vector<TokenStream> stream_stack;  ///< Internal storage of sub-stack strings
		std::vector<SubStreamType> fallback_types; ///< Additional stream stack information that ensures maching entering and exiting
		bool skip_till_fallback = false;

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

		/**
		 * @brief Informs whether new errors and some parsing should be skipped till fallback is reached.
		 */
		[[nodiscard]] 
		bool isSkipping() const;

		// clang-format off
		[[nodiscard]]
		const Token& operator[](i64 fwd) const {
			return ctokens().peek(fwd);
		}

		// clang-format on

		Ref<dia::Logger>      err;      ///< Stores parsing errors
		MRef<dia_int::Logger> int_err;  ///< Stores parsing errors

		ParserState(TokenStream&& tokens, Ref<dia::Logger> err, MRef<dia_int::Logger> int_err = {}):
			  err(err),
			  int_err(int_err) {
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
		 * @brief deletes current stream and makes last stream the current stream. Resets error bit(Additional errors are no longer ignored). This will produce an error if the whole sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void goUp();
		/**
		 * @brief deletes current stream and makes last stream the current stream then skips one token(the recursive token that was the source of the deleted stream). Resets error bit(Additional errors are no longer ignored). This will produce an error if the whole sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void goUpAndSkip();

		/**
		 * @brief Creates a new sub-stream of given length starting in the current token.
		 */
		void setFallback(u64 length);

		/**
		 * @brief Goes back from the fallback sub-stream to the fallback position. Resets error bit(Additional errors are no longer ignored). This will produce an error if the whole sub-stream wasn't parsed and an error wasn't emitted.
		 */
		void exitFallback();

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void fail(i64 rel_pos, const std::string& message) {
			err->failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

		/**
		 * @brief Logs an error relatively to the current token
		 */
		void log(Box<dia::Message> message) { err->log(std::move(message)); }

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
