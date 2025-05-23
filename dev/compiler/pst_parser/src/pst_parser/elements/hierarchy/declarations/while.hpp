#pragma once

#include "../not_statements.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief While declaration
	 */
	class While final: public CodeDecl {
		AccessInternal<RoundGroupExpr>  condition;
		tpc::OptionalIdentifier         optional_name;
		AccessInternal<CodeBlockOrStmt> body;

	public:
		explicit While(const dia::SourcePosition& position): CodeDecl(position) {
			element_kind = ElementKind::While;
		}

		static MBox<While> parse(LangParserState& state);
		void               dprint(std::ostream& out) const final;
		~While() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "While";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
