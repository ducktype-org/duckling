#include "hir_expr.hpp"
#include "symtable/symbol_ref.hpp"
#include <base/exceptions.hpp>
#include <base/variant.hpp>
#include <base/str_to_int.hpp>
#include <typesystem/typesystem.hpp>

#include <exec/operators/builtinoperators.hpp>
#include <operations/operation.hpp>
#include <utility>
#include <utility>

namespace hir {

	class SymbolExpr: public Expression {
		base::StrId                        name;
		std::optional<symtable::SymbolRef> symbol;

	public:
		SymbolExpr(symtable::ScopeRef scope, base::StrId name):
			  Expression(std::move(scope)),
			  name(std::move(name)) {}

		void lookup([[maybe_unused]]AnalysisState& state) final {
			if (lookup_done) return;
			lookup_done = true;

			auto lookup_result = scope->lookupMeAndParents(name);
			std::cerr << "   FULL LK RES: ";
			lookup_result.dprint(std::cerr);
			std::cerr << "\n";
			if (!lookup_result.isSingle())
				RIFT_PANIC("ambiguity in expr lookup, @TODO: error in state");
			auto as_single = lookup_result.getAsSingle();

			std::cerr << "   SYMBOL EXPR RES: ";
			symtable::dprintSymbolChain(as_single, std::cerr);
			std::cerr << "\n";

			auto dealiased_single = symtable::deAliasSymbolChain(as_single);

			std::cerr << "   SYMBOL EXPR RES DEALIASED: ";
			symtable::dprintSymbolChain(dealiased_single, std::cerr);
			std::cerr << "\n";

			symbol = dealiased_single.back();
			std::cerr << "   SYMBOL : " << symbol.value()->getName().strView() << "\n\n";
		}

		void determineType([[maybe_unused]]AnalysisState& state) final {
			if (type_done) return;
			type_done = true;

			lookup(state);
			type = symbol.value()->getType();
		}

		exec::CTV eval([[maybe_unused]]AnalysisState& state) final {
			// @TODO: this should be called just once
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
			  Expression(std::move(scope)),
			  value(value) {}

		void lookup([[maybe_unused]]AnalysisState& state) final { lookup_done = true; }

		void determineType([[maybe_unused]]AnalysisState& state) final {
			if (type_done) return;
			type_done = true;
			type      = ts::TypeDesc<>(ts::IntegralInfo::create(32));
		}

		exec::CTV eval(AnalysisState& state) final {
			determineType(state);
			exec::CTV out              = exec::alloc_new(getType(state));
			out.getData<i32>().front() = value;
			return out;
		}
	};

	class LiteralTypeExpr: public Expression {
		ts::TypeDesc<> type_value;

	public:
		LiteralTypeExpr(symtable::ScopeRef scope, ts::TypeDesc<> type):
			  Expression(std::move(scope)),
			  type_value(type) {}

		void lookup([[maybe_unused]]AnalysisState& state) final { lookup_done = true; }

		void determineType([[maybe_unused]]AnalysisState& state) final {
			if (type_done) return;
			type_done = true;
			type      = ts::TypeDesc<>(ts::MetaInfo::create());
		}

		ts::TypeDesc<> evalAsType(AnalysisState& state) final {
			determineType(state);
			return type_value;
		}
	};

	class BinOperatorExpr: public Expression {
		ExpressionRef lhs;
		ExpressionRef rhs;
		base::StrId   oper;

		std::optional<operation::OperationId> operation_id;

	public:
		BinOperatorExpr(
			symtable::ScopeRef scope, ExpressionRef lhs, ExpressionRef rhs, base::StrId oper
		):
			  Expression(std::move(scope)),
			  lhs(std::move(lhs)),
			  rhs(std::move(rhs)),
			  oper(std::move(oper)) {}

		void lookup(AnalysisState& state) final {
			if (lookup_done) return;
			lookup_done = true;

			lhs->lookup(state);
			rhs->lookup(state);
		}

		void determineType(AnalysisState& state) final {
			if (type_done) return;
			type_done = true;

			lookup(state);
			lhs->determineType(state);
			rhs->determineType(state);

			exec::Operator exec_operator{0};
			if (oper == base::StrId('+'))
				exec_operator = exec::Operator::Plus;
			else if (oper == base::StrId('-'))
				exec_operator = exec::Operator::Minus;
			else
				throw base::NotYetImplemented("Operator different then + or -");

			exec::BuiltInOp builtin_op{ exec_operator,
				                        { lhs->getType(state), rhs->getType(state) } };

			if (not exec::getBuiltInOps().contains(builtin_op))
				RIFT_PANIC("BinOperatorExpr encountered expression that is not builtin");

			operation_id                = exec::getBuiltInOps().at(builtin_op);
			const auto& typed_operation = operation::getOperation(operation_id.value());

			type = typed_operation.signature.getResultType();
		}

		exec::CTV eval(AnalysisState& state) final {
			determineType(state);

			auto lhs_result = lhs->eval(state);
			auto rhs_result = rhs->eval(state);

			const auto& typed_operation = operation::getOperation(operation_id.value());

			return typed_operation.function({ lhs_result, rhs_result });
		}
	};

	// all other types like: lambda

	ExpressionRef makeFromKeyword(const symtable::ScopeRef& scope, rift_def::Keyword keyword) {
		switch (keyword) {
		case rift_def::Keyword::i32:
			// @TODO: signedness
			return base::make_unique<LiteralTypeExpr>(
				scope, ts::TypeDesc<>(ts::IntegralInfo::create(32))
			);

		default:
			// @TODO: errors
			RIFT_PANIC("Bad keyword in hir expr");
		}
	}

	ExpressionRef makeFromSingle(const symtable::ScopeRef& scope, const pst::Expr::ExprElem& elem) {
		variant_match(elem) {
			variant_case(pst::Expr::KeywordValue, key) {
				return makeFromKeyword(scope, key.keyword);
			}
			variant_case(pst::Expr::NumLiteral, num) {
				auto val = base::strIdToNum(num.num_id);
				return base::make_unique<LiteralIntExpr>(scope, val);
			}
			variant_case(pst::Expr::Identifier, identifier) {
				return base::make_unique<SymbolExpr>(scope, identifier.indent_id);
			}
			variant_case(pst::Expr::Group, group) {
				// @TODO: take type into consideration
				return Expression::makeExpr(scope, group.expr.borrow());
			}
			variant_case_novalue(pst::Expr::Operator) {
				RIFT_PANIC("Expression consisting of only operator is not allowed.");
			}
			variant_default {
				// @TODO
				return nullptr;
			}
		}
		RIFT_PANIC("Some case did not return");
	}

	ExpressionRef Expression::makeExpr(
		const symtable::ScopeRef& scope, const pst::ParserCBorrowRef<pst::Expr>& pst_expr
	) {
		// temporary:
		// @TODO: proper algorithm

		if (pst_expr->elements.size() == 1) {
			auto& elem = pst_expr->elements[0];
			return makeFromSingle(scope, elem);
		} else if (pst_expr->elements.size() == 3) {
			// This assumes that it is expr as <value operator value>
			auto lhs = makeFromSingle(scope, pst_expr->elements[0]);
			auto rhs = makeFromSingle(scope, pst_expr->elements[2]);

			// @TODO: errors:
			auto oper = std::get<pst::Expr::Operator>(pst_expr->elements[1]).oper_id;

			return base::make_unique<BinOperatorExpr>(scope, std::move(lhs), std::move(rhs), oper);
		} else {
			throw base::NotYetImplemented(base::strConcat(
				"Make Hir Expr for expressions of length ", pst_expr->elements.size(), "."
			));
		}

		RIFT_PANIC("Some case did not return");
	}

	ts::TypeDesc<> Expression::getType(AnalysisState& state) {
		if (!type.has_value()) determineType(state);
		RIFT_ASSERT(type.has_value(), "Type determination failed");
		return *type;
	}

	void Expression::determineType([[maybe_unused]]AnalysisState& state) {
		RIFT_PANIC("Called determineType on expression not implementing it");
	}

	void Expression::lookup([[maybe_unused]]AnalysisState& state) {
		RIFT_PANIC("Called lookup on expression not implementing it");
	}

	ts::TypeDesc<> Expression::evalAsType([[maybe_unused]]AnalysisState& state) {
		RIFT_PANIC("Called evalAsType on expression not implementing it");
	}

	exec::CTV Expression::eval([[maybe_unused]]AnalysisState& state) {
		RIFT_PANIC("Called eval on expression not implementing it");
	}

}
