#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Comma separated expression.
	 *
	 * @note This is the second possible entry point for expression parsing when comma
	 * expression doesn't cause problems with other parsing.
	 */
	class Comma final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Comma, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		using Lower = Ternary;

		std::vector<AccessInternalAnonymous<ExprElement>> expressions;

	public:
		explicit Comma(const LangParserState& state): ExprElement(state, 900) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Comma Expr";
		}

		static MBox<ExprElement> parse(LangParserState& state);

		~Comma() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		auto getExpressions() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<ExprElement> { return ref.give(); };
			return std::ranges::ref_view(expressions) | transform(give_one);
		}

		void calcElementPathHashRecursive() override;
	};
}
