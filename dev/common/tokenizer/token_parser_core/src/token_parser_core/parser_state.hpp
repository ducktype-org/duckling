#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/source_position.hpp>
#include "token_stream.hpp"
#include "base_element.hpp"
#include "common_elements.hpp"

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

	template<typename Automatic>
	class AutomatedParserState: public ParserState {
	public:
		template<typename... Ts>
		AutomatedParserState(Ts&&... args): ParserState(std::forward<Ts>(args)...) {} 

		virtual Automatic parse() = 0;
	};

	template<typename State>
	class GenericAutomatic {
	protected:
		State& state;
	public:
		GenericAutomatic(State& state):state(state) {}
		GenericAutomatic(const GenericAutomatic&) = delete;

		/**
	 	* @brief Parses the expected keyword. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param key The expected keyword.
	 	*/
		void one(Keyword key, bool ignorable = false);

		/**
	 	* @brief Parses the expected Special token. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param spec The expected special token.
	 	*/
		void one(Special spec, bool ignorable = false);

		/**
	 	* @brief Parses the expected operator. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param op The expected operator.
	 	*/
		void one(Operator op, bool ignorable = false);

		/**
	 	* @brief Parses an identifier to @p result. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param result The place to store the parsed identifier.
	 	*/
		void one(Identifier* result, bool ignorable = false);

		/**
	 	* @brief Parses an identifier to @p result. Skips on success, does nothing on failure.
	 	* @param state The current ParserState.
	 	* @param result The place to store the parsed identifier.
	 	*/
		void one(OptionalIdentifier* result, bool ignorable = false);

		/**
	 	* @brief Parses an Element. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param result The place to store the parsed element.
	 	*/
		template<std::derived_from<Element> T>
		void one(ParserRef<T>* result, bool = false) {
			*result = T::parse(state);
		}

		/**
	 	* @brief Parses an Element. Skips on success, logs error on failure.
	 	* @param state The current ParserState.
	 	* @param result The place to store the parsed element.
	 	*/
		template<std::derived_from<Element> T>
		void one(base::Optional<ParserRef<T>>* result, bool = false) {
			*result = T::parse(state);
		}

		// parses all the given elements
		template<typename T>
		void all(T t) {
			one(t);
		}

		template<typename T, typename... Q>
		void all(T t, Q... q) {
			one(t);
			parseRest(q...);
		}

	private:
		// parses all the given elements
		template<typename T>
		void parseRest(T t) {
			one(t, true);
		}

		template<typename T, typename... Q>
		void parseRest(T t, Q... q) {
			one(t, true);
			parseRest(q...);
		}
	};
}
