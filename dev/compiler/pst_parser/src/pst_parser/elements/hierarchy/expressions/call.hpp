#pragma once

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Represents a single call or subscript expression
	 */
	class Call final: public ExprElement {
		lexer::Token::BracketType type
			= lexer::Token::BracketType::None;  ///< either Round or Square
		AccessInternal<CallList> args;

	public:
		Call(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~Call() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

		[[nodiscard]] lexer::Token::BracketType getType() const;

		[[nodiscard]] AccessLocked<CallList> getArgs() const { return args.give(); }

		[[nodiscard]]
		std::string elementType() const override {
			return "Call Expression";
		}
	};
}
