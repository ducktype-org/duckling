#pragma once

#include "token_stream.hpp"
#include "parser_ref.hpp"

#include <error_state/error_state.hpp>

namespace tpc {

	class ParserState {
		std::vector<TokenStream> stream_stack;

	public:
		TokenStream&       tokens();
		const TokenStream& ctokens() const;

		ErrorState err;

		ParserState(TokenStream&& tokens, ErrorState&& err): err(std::move(err)) {
			stream_stack.emplace_back(std::move(tokens));
		}

		bool empty() const;
		bool notEmpty() const;

		void goDown();
		void goUp();
		void goUpAndSkip();

		void fail(usize rel_pos, std::string message) {
			err.failAndLog(ctokens().peek(rel_pos).getPosition(), message);
		}

		/**
		 * @return true if ate
		 * @return false if didn't eat
		 */
		bool tryEat(Keyword key) {
			if (tokens().is(key)) {
				tokens().next();
				return true;
			}
			return false;
		}

		bool tryEat(Special spec) {
			if (tokens().is(spec)) {
				tokens().next();
				return true;
			}
			return false;
		}

		bool tryEat(Operator op) {
			if (tokens().is(op)) {
				tokens().next();
				return true;
			}
			return false;
		}
	};

}
