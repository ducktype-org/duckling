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
	protected:
		/**
		 * @brief Types of substream:
		 * - Recursive - Comes from the token structure.
		 * - NonRecursive - Manually set fallback.
		 */
		enum SubStreamType { Recursive, NonRecursive };

		/**
		 * @brief Data needed to handle restoring to a fallback
		 */
		struct Fallback {
			SubStreamType type;
			Box<TokenStream>   saved_stream;
			/**
			 * @brief Jump done after restoring a fallback.
			 */
			u64 post_jump;
		};

		Box<TokenStream> current_stream;

		std::vector<Fallback> fallback_stack;  ///< Internal storage of fallback token streams

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
		const Token& operator[](i64 fwd) const {
			return ctokens().peek(fwd);
		}

		// clang-format on

		Ref<dia::Logger>      err;      ///< Stores parsing errors
		MRef<dia_int::Logger> int_err;  ///< Stores parsing errors

		ParserState(TokenStream&& tokens, Ref<dia::Logger> err, MRef<dia_int::Logger> int_err = {}):
			  current_stream(makeBox<TokenStream>(std::move(tokens))),
			  fallback_stack(),
			  err(err),
			  int_err(int_err) {}

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
		 * current stream.
		 */
		virtual void goDown();
		/**
		 * @brief deletes current stream and makes last stream the current stream.
		 */
		virtual void goUp();
		/**
		 * @brief deletes current stream and makes last stream the current stream then skips one
		 * token.
		 */
		virtual void goUpAndSkip();

		/**
		 * @brief Logs an error relatively to the current token. 
		 * @note This version is deprecated in favor of the diagnostic Message system.
		 */
		[[deprecated]]
		virtual void fail(i64 rel_pos, const std::string& message) {
			err->failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

		/**
		 * @brief Logs an error relatively to the current token.
		 */
		virtual void log(Box<dia::Message> message) { err->log(std::move(message)); }

		template<TokenStreamCondition until>
		[[nodiscard]]
		u64 countUntil() const {
			return current_stream->countUntil<until>();
		}

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
