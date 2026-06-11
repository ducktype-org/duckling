#pragma once
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents an binding pattern, which binds a value to a new variable.
	 */
	class BindingPattern final: public AnalysisPattern {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(BindingPattern, AnalysisPattern);
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(name, IdentifierWrapper);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit BindingPattern(const LangParserState& state): AnalysisPattern(state) {
			this->element_kind = ElementKind::BindingPattern;
		}

		~BindingPattern() final = default;

		static MBox<BindingPattern> parse(LangParserState& state);
		void                        dprint(std::ostream& out) const final;

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Binding Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
