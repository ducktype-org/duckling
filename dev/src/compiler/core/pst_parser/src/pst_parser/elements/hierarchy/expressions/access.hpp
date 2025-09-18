#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This represents a single access expression of type `[expression operator like . or
	 * .?][name][optionally template specifier]`
	 */
	class Access final: public ExprElement {
		base::StrID     type;  ///< either `.` or `.?`
		tpc::Identifier name;
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		Access(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~Access() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& calcStableHash(HashAlg&) const override;

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
