#pragma once
#include "../../meta.hpp"
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a flow pattern.
	 */
	class FlowPattern final: public NotStmt {
		base::Optional<tpc::Identifier> as_identifier;
		NAMED_CHILD(pattern, AnalysisPattern);
		NAMED_CHILD_OPT(type_constraint, UniversalExprHolder);

	protected:
		HashAlg& calcStableHash(HashAlg&) const override;
	public:
		explicit FlowPattern(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::FlowPattern;
		}

		~FlowPattern() final = default;

		[[nodiscard]] AccessLocked<AnalysisPattern> getPattern() const { return pattern.give(); }

		[[nodiscard]] base::Optional<base::StrID> getAsIdentifier() const {
			return as_identifier->value;
		}

		[[nodiscard]] base::Optional<AccessLocked<UniversalExprHolder>> getTypeConstraint() const {
			return type_constraint.map([](const auto& value) { return value.give(); });
		}

		static MBox<FlowPattern> parse(LangParserState& state);
		void                     dprint(std::ostream& out) const final;

		[[nodiscard]] std::string elementType() const override { return "Flow Pattern"; }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
