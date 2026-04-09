#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This represents a single access expression of type `[expression operator like . or
	 * .?][name][optionally template specifier]`
	 */
	class Access final: public ExprElement {
		base::StrID     type;  ///< either `.` or `.?` or `::`
		tpc::Identifier name;
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		Access(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~Access() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Access Expression";
		}

		[[nodiscard]]
		base::StrID getType() const;
		[[nodiscard]]
		const tpc::Identifier& getName() const;

		[[nodiscard]]
		base::Optional<AccessLocked<ExprElement>> getTemplateSpecifier() const {
			return template_specifier.map([](const auto& t) { return t.give(); });
		}
	};
}
