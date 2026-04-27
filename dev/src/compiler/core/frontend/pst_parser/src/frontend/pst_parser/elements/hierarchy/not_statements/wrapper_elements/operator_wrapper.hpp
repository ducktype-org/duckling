#pragma once

#include "../../meta.hpp"

namespace pst {
	class OperatorWrapper final: public NotStmt {
		lexer::Operator op;

	public:
		explicit OperatorWrapper(LangParserState& state, lexer::Operator op):
			  NotStmt(state),
			  op(op) {
			element_kind = ElementKind::OperatorWrapper;
		}

		static MBox<OperatorWrapper> parse(LangParserState& state);
		~OperatorWrapper() final = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		lexer::Operator getOperator() const {
			return op;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Operator Wrapper";
		}
	};
}
