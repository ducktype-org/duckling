#include "type_name_extractor.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace {
	/// Tiny visitor that extracts a type name from a PST type expression
	/// (currently only bare identifiers like `i64`/`bool` are supported).
	/// Uses the Empty base so non-identifier nodes are silently ignored — the caller
	/// inspects the resulting `name` and decides whether extraction succeeded.
	class TypeNameExtractor final: public pst::expr::PstExprVisitorEmpty {
	public:
		base::StrID name{};

		void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) override {
			name = ident->getName().illegalAccess().value()->unwrap();
		}

		// Built-in primitive type names (`i64`, `bool`, `void`, ...) are tokenized as keywords.
		void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) override {
			name = lang_def::keywordToStr(kw->getKeyword().illegalAccess().value()->unwrap());
		}
	};
}

namespace pg {
	base::StrID extractTypeName(const pst::ExprElement& expr) {
		TypeNameExtractor extractor;
		expr.acceptExprVisitor(extractor);
		return extractor.name;
	}
}
