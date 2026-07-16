#pragma once
#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Base class for analysis patterns.
	 */
	class AnalysisPattern: public NotStmt {
		THIS_CLASS(AnalysisPattern);
		PARENT_CLASS(NotStmt);

	public:
		ELEMENT_CLONE_DECL(AnalysisPattern);

		explicit AnalysisPattern(LangElementConstructionArgument state): NotStmt(state) {
			this->element_kind = ElementKind::AnalysisPattern;
		}

		virtual ~AnalysisPattern() = default;

		static MBox<AnalysisPattern> parse(LangParserState& state);

		[[nodiscard]]
		std::string elementType() const override {
			return "Analysis Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
