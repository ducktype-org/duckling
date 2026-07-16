#pragma once
#include "../../meta.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a flow pattern.
	 */
	class FlowPattern final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FlowPattern, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(pattern, AnalysisPattern);
		NAMED_CHILD_OPT(as_identifier, IdentifierWrapper);
		NAMED_CHILD_OPT(type_constraint, UniversalExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit FlowPattern(LangElementConstructionArgument state): NotStmt(state) {
			this->element_kind = ElementKind::FlowPattern;
		}

		~FlowPattern() final = default;

		[[nodiscard]]
		AccessLocked<AnalysisPattern> getPattern() const {
			return pattern.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getAsIdentifier() const {
			return as_identifier.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]] base::Optional<AccessLocked<UniversalExprHolder>> getTypeConstraint() const;

		static MBox<FlowPattern> parse(LangParserState& state);
		void                     dprint(std::ostream& out) const final;

		[[nodiscard]] std::string elementType() const override { return "Flow Pattern"; }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
