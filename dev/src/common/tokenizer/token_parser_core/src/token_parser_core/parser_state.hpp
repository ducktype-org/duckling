#pragma once

#include "token_stream.hpp"

#include <diagnostic_interactive/logger_fwd.hpp>
#include <diagnostic_interactive/message.hpp>

#include <diagnostic/source_position.hpp>

namespace tpc {

	/**
	 * @brief Lightweight parsing context.
	 * @note It is used during parsing, and can be changed during this process,
	 *       in particular when passing the context to subelements beeing parsed.
	 *       It can also be restored on exiting a fallback.
	 *
	 * @note This is a base class, the data depends on the needs of the parser.
	 */
	class ParserContext {
	public:
		ParserContext() = default;

		[[nodiscard]]
		virtual Box<ParserContext> copy() const {
			return makeBox<ParserContext>();
		}

		virtual ~ParserContext() = default;
	};

	/**
	 * @brief Implements higher level token stream interactions.
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
		 * @brief Data needed to handle restoring to a fallback.
		 */
		struct Fallback final {
			SubStreamType                                         type;
			Box<TokenStream>                                      saved_stream;
			std::variant<Box<ParserContext>, CRef<ParserContext>> saved_context;
			/**
			 * @brief Jump done after restoring a fallback stored as a set jump length.
			 */
			u64 post_jump;
		};

		/**
		 * @brief Data needed to handle restoring to a fallback. This type of fallback doesn't
		 * define a specific length, instead if error is encountered it uses the function saved
		 * under fail_jump to try to restore a proper parsing context
		 *
		 * Right now mostly used for flow control elements as the complexity of finding the end of
		 * such a statement is non-trivial.
		 */
		struct SoftFallback final {
			std::variant<Box<ParserContext>, CRef<ParserContext>> saved_context;
			/**
			 * @brief Condition to be skipped to on error.
			 *
			 * The condition should only be used where specific place is hard to compute, mostly
			 * flow control elements as the versions without brackets are hard to handle on error.
			 *
			 * The condition version only uses the condition to jump on failure.
			 */
			std::function<TokenStreamCondition> fail_jump;
		};

		Box<TokenStream> current_stream;

		std::vector<std::variant<Fallback, SoftFallback>>
			fallback_stack;  ///< Internal storage of fallback token streams

		std::variant<Box<ParserContext>, CRef<ParserContext>> current_context;

		ParserState(TokenStream&& tokens, Box<ParserContext>&& ctx, Ref<dia_int::Logger> int_err):
			  current_stream(makeBox<TokenStream>(std::move(tokens))),
			  fallback_stack(),
			  current_context(std::move(ctx)),
			  int_err(int_err) {}


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

		Ref<dia_int::Logger> int_err;  ///< Stores parsing errors

		ParserState(TokenStream&& tokens, Ref<dia_int::Logger> int_err):
			  current_stream(makeBox<TokenStream>(std::move(tokens))),
			  fallback_stack(),
			  current_context(makeBox<ParserContext>()),
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

		virtual void logInt(Box<dia_int::MessageBase> message) { int_err->log(std::move(message)); }

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

		virtual ~ParserState() = default;
	};
}
