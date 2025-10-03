#pragma once
#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Base class for analysis patterns.
	 */
	class AnalysisPattern: public NotStmt {
	public:
		explicit AnalysisPattern(const dia::SourcePosition& position): NotStmt(position) {
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
