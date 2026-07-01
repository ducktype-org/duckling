#pragma once

#include "../lists/call_list.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Represents a single call or subscript expression
	 */
	class Call final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Call, ExprElement, type);
		CLONE_SUBELEMENTS();

	protected:
		lexer::Token::BracketType type
			= lexer::Token::BracketType::None;  ///< either Round or Square
		NAMED_CHILD(args, CallList);

	public:
		Call(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~Call() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]] lexer::Token::BracketType getType() const;

		[[nodiscard]] AccessLocked<CallList> getArgs() const { return args.give(); }

		[[nodiscard]]
		std::string elementType() const override {
			return "Call Expression";
		}
	};
}
