#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This represents a single access expression of type `[expression operator like . or
	 * .?][name][optionally template specifier]`
	 */
	class Access final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Access, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(type, OperatorWrapper);  ///< either `.` or `.?` or `::`
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		explicit Access(LangElementConstructionArgument state): ExprElement(state, 300) {}

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
		AccessLocked<OperatorWrapper> getType() const {
			return type.give();
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprElement>> getTemplateSpecifier() const {
			return template_specifier.map([](const auto& t) { return t.give(); });
		}
	};
}
