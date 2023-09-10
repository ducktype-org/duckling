#include "hir_expr.hpp"
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <base/str_to_int.hpp>
#include <typesystem/typesystem.hpp>

// @Placeholder

namespace hir {

	class SymbolExpr: public Expression {};

	class LiteralIntExpr: public Expression {
		// @TODO: some „BigInt” in the future here
		// perhaps we can treat it like string as long as possible
		i32 value;
	
	public:
		LiteralIntExpr(i32 value): value(value) {}

		void determineType() final {
			// @TODO: this 32 is just temporary:
			type = ts::TypeDesc<>(ts::IntegralInfo::create(32));
		}
		exec::CTV eval() final {
			exec::CTV out = exec::alloc_new(getType());
			out.getData<i32>().front() = value;
			return out;
		}
	};

	class LiteralTypeExpr: public Expression {
		ts::TypeDesc<> type_value;
	
	public:
		LiteralTypeExpr(ts::TypeDesc<> type): type_value(type) {}
		void determineType() final {
			type = ts::TypeDesc<>(ts::MetaInfo::create());
		}
		ts::TypeDesc<> evalAsType() final {
			return type_value;
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
		// @TODO: proper algorithm

		if (pst_expr->elements.size() == 1) {
			auto& elem = pst_expr->elements[0];
			variant_match(elem) {
				variant_case (pst::Expr::KeywordValue, key) {
					return makeFromKeyword(key.keyword);
				}
				variant_case(pst::Expr::NumLiteral, num) {
					auto val = base::strIdToNum(num.num_id);
					return base::make_unique<LiteralIntExpr>(val);
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

	ts::TypeDesc<> Expression::getType() {
		if (!type.has_value()) {
			determineType();
		}
		RIFT_ASSERT(type.has_value(), "Type determination failed");
		return *type;
	}

	void Expression::determineType() {
		RIFT_PANIC("Called determineType on expression not implementing it");
	}

	ts::TypeDesc<> Expression::evalAsType() {
		RIFT_PANIC("Called evalAsType on expression not implementing it");
	}

	exec::CTV Expression::eval() {
		RIFT_PANIC("Called eval on expression not implementing it");
	}

}
