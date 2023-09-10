#include "hir_expr.hpp"
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <typesystem/typesystem.hpp>

// @Placeholder

namespace hir {

	class SymbolExpr: public Expression {};

	class LiteralExpr: public Expression {};

	class LiteralTypeExpr: public Expression {
		ts::TypeDesc<> type;
	
	public:
		LiteralTypeExpr(ts::TypeDesc<> type): type(type) {}
		ts::TypeDesc<> evalAsType() final {
			return type;
		}
	};

	class OperatorExpr: public Expression {};

	// all other types like: lambda

	ExpressionRef makeFromKeyword(rift_def::Keyword keyword) {
		switch (keyword) {
		case rift_def::Keyword::i32:
			// @TODO: signedness 
			return base::make_unique<LiteralTypeExpr>(ts::TypeDesc<>(ts::IntegralInfo::create(32)));	
	
		default:
			// @TODO: errors 
			RIFT_PANIC("Bad keyword in hir expr");
		}
	}

	ExpressionRef Expression::makeExpr(pst::ParserCBorrowRef<pst::Expr> pst_expr) {
		
		// temporary:

		if (pst_expr->elements.size() == 1) {
			// @TODO
			auto& elem = pst_expr->elements[0];
			variant_match(elem) {
				variant_case (pst::Expr::KeywordValue, key) {
					return makeFromKeyword(key.keyword);
				}
				variant_default {
					// @TODO
					return nullptr;
				}
			}
		}
		else {
			throw base::NotYetImplemented("Make Hir Expr for longer expressions");
		}

		RIFT_PANIC("Some case did not return");
	}

	ts::TypeDesc<> Expression::evalAsType() {
		RIFT_PANIC("Called evalAsType on expression not implementing it");
	}

}
