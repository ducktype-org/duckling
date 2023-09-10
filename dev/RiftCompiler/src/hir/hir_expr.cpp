#include "hir_expr.hpp"
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <base/str_to_int.hpp>
#include <typesystem/typesystem.hpp>
#include "symtable/symbol_ref.hpp"

// @Placeholder

namespace hir {

	class SymbolExpr: public Expression {
		base::StrId name;
		std::optional<symtable::SymbolRef> symbol;
	public:
		SymbolExpr(symtable::ScopeRef scope, base::StrId name):
			Expression(scope), name(name) {}

		void lookup(AnalysisState& state) final {
			auto lookup_result = scope->lookupMeAndParents(state, name);
			if (!lookup_result.isSingle()) {
				RIFT_PANIC("ambiguity in expr lookup, @TODO: error in state");
			}
			auto as_single = lookup_result.getAsSingle();
			
			std::cerr << "   SYMBOL EXPR RES: ";
			symtable::dprintSymbolChain(as_single, std::cerr);
			std::cerr << "\n";

			auto dealiased_single = symtable::deAliasSymbolChain(state, as_single);

			std::cerr << "   SYMBOL EXPR RES DEALIASED: ";
			symtable::dprintSymbolChain(dealiased_single, std::cerr);
			std::cerr << "\n";

			symbol = dealiased_single.back();
			std::cerr << "SYMBOL : " << symbol.value()->getName().strView() << "\n";
		}
		void determineType(AnalysisState& state) final {
			lookup(state);
			type = symbol.value()->getType();
		}
		exec::CTV eval(AnalysisState& state) final {
			determineType(state);
			return symbol.value()->getValue();
		}
	};

	class LiteralIntExpr: public Expression {
		// @TODO: some „BigInt” in the future here
		// perhaps we can treat it like string as long as possible
		i32 value;
	
	public:
		LiteralIntExpr(symtable::ScopeRef scope, i32 value):
			Expression(scope), value(value) {}
		
		void lookup(AnalysisState& state) final {}
		void determineType(AnalysisState& state) final {
			// @TODO: this 32 is just temporary:
			type = ts::TypeDesc<>(ts::IntegralInfo::create(32));
		}
		exec::CTV eval(AnalysisState& state) final {
			exec::CTV out = exec::alloc_new(getType(state));
			out.getData<i32>().front() = value;
			return out;
		}
	};

	class LiteralTypeExpr: public Expression {
		ts::TypeDesc<> type_value;
	
	public:
	
		void lookup(AnalysisState& state) final {}
		LiteralTypeExpr(symtable::ScopeRef scope, ts::TypeDesc<> type):
			Expression(scope), type_value(type) {}
		void determineType(AnalysisState& state) final {
			type = ts::TypeDesc<>(ts::MetaInfo::create());
		}
		ts::TypeDesc<> evalAsType(AnalysisState& state) final {
			return type_value;
		}
	};

	class OperatorExpr: public Expression {};

	// all other types like: lambda

	ExpressionRef makeFromKeyword(symtable::ScopeRef scope, rift_def::Keyword keyword) {
		switch (keyword) {
		case rift_def::Keyword::i32:
			// @TODO: signedness 
			return base::make_unique<LiteralTypeExpr>(
				scope,
				ts::TypeDesc<>(ts::IntegralInfo::create(32))
			);
	
		default:
			// @TODO: errors 
			RIFT_PANIC("Bad keyword in hir expr");
		}
	}

	ExpressionRef Expression::makeExpr(symtable::ScopeRef scope, pst::ParserCBorrowRef<pst::Expr> pst_expr) {
		
		// temporary:
		// @TODO: proper algorithm

		if (pst_expr->elements.size() == 1) {
			auto& elem = pst_expr->elements[0];
			variant_match(elem) {
				variant_case (pst::Expr::KeywordValue, key) {
					return makeFromKeyword(scope, key.keyword);
				}
				variant_case(pst::Expr::NumLiteral, num) {
					auto val = base::strIdToNum(num.num_id);
					return base::make_unique<LiteralIntExpr>(scope, val);
				}
				variant_case(pst::Expr::Identifier, identifier) {
					return base::make_unique<SymbolExpr>(scope, identifier.indent_id);
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

	ts::TypeDesc<> Expression::getType(AnalysisState& state) {
		if (!type.has_value()) {
			determineType(state);
		}
		RIFT_ASSERT(type.has_value(), "Type determination failed");
		return *type;
	}

	void Expression::determineType(AnalysisState& state) {
		RIFT_PANIC("Called determineType on expression not implementing it");
	}

	void Expression::lookup(AnalysisState& state) {
		RIFT_PANIC("Called lookup on expression not implementing it");
	}

	ts::TypeDesc<> Expression::evalAsType(AnalysisState& state) {
		RIFT_PANIC("Called evalAsType on expression not implementing it");
	}

	exec::CTV Expression::eval(AnalysisState& state) {
		RIFT_PANIC("Called eval on expression not implementing it");
	}

}
