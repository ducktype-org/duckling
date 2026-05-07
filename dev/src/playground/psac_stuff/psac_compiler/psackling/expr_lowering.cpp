#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/binary_operator.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/comparison_chain.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/round_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <lang_definitions/key_spec_op.hpp>

#include "expr_lowering.hpp"

#include <base/except/exceptions.hpp>

namespace pg {
	LoweredExpr ExprLowering::lower(const pst::ExprElement& expr) {
		result = {};
		expr.acceptExprVisitor(*this);
		return result;
	}

	void ExprLowering::visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> lit) {
		// M3: only i64 numeric literals are supported.
		const u64   value   = std::stoull(lit->getValue().str());
		llvm::Type* llvm_ty = lowering.lowerType(ctx.i64_type_id, ctx);
		result              = LoweredExpr{
		             .value   = llvm::ConstantInt::get(llvm_ty, value),
		             .type_id = ctx.i64_type_id,
        };
	}

	void ExprLowering::visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) {
		// In expression position only the boolean literals are meaningful for now.
		// Type-name keywords (`i64`, `bool`, ...) only appear in type position and are
		// extracted by FunctionPass before reaching the expression lowering.
		llvm::Type* i1_ty = lowering.lowerType(ctx.bool_type_id, ctx);
		switch (kw->getKeyword()) {
			case lang_def::Keyword::True:
				result = LoweredExpr{
					.value   = llvm::ConstantInt::get(i1_ty, 1),
					.type_id = ctx.bool_type_id,
				};
				return;
			case lang_def::Keyword::False:
				result = LoweredExpr{
					.value   = llvm::ConstantInt::get(i1_ty, 0),
					.type_id = ctx.bool_type_id,
				};
				return;
			default:
				throw std::runtime_error(
					"Keyword '" + lang_def::keywordToStr(kw->getKeyword()).str()
					+ "' is not a valid value expression"
				);
		}
	}

	void ExprLowering::visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) {
		const base::StrID name      = ident->getName().value;
		sym::SymbolID     sym_id    = ctx.lookupInScopeAndParents(ctx.currNs().id, name);
		sym::Symbol&      symbol    = ctx.getSymbol(sym_id);

		if (!std::holds_alternative<sym::VariableSymbol>(symbol.kind)) {
			CORE_PANIC(
				"Identifier '" + name.str()
				+ "' is not a variable (other symbol kinds not yet supported in expressions)"
			);
		}

		if (!locals.contains(sym_id)) {
			CORE_PANIC(
				"Variable '" + name.str() + "' has no associated stack slot (locals map miss)"
			);
		}
		llvm::AllocaInst* slot = locals[sym_id];

		llvm::Type* llvm_ty = lowering.lowerType(symbol.type_id, ctx);
		result              = LoweredExpr{
		             .value   = builder.CreateLoad(llvm_ty, slot, name.strView()),
		             .type_id = symbol.type_id,
        };
	}

	void ExprLowering::visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> op) {
		LoweredExpr lhs = lower(*op->getLeftOperand().illegalAccess().value());
		LoweredExpr rhs = lower(*op->getRightOperand().illegalAccess().value());

		// Logical `and`/`or` come from PST as BinaryOperator subclasses (LogicAnd/LogicOr)
		// whose operator is a keyword (StrID "and"/"or") — asNamed() reports NotAnOperator,
		// so we match by raw operator StrID.
		const base::StrID op_str = op->getOperator().value;
		if (op_str == lang_def::keywordToStr(lang_def::Keyword::And)
		    || op_str == lang_def::keywordToStr(lang_def::Keyword::Or)) {
			if (lhs.type_id != ctx.bool_type_id || rhs.type_id != ctx.bool_type_id) {
				throw std::runtime_error(
					"Logical '" + op_str.str() + "' requires bool operands"
				);
			}
			// No short-circuit yet; both sides are already evaluated above.
			llvm::Value* out = (op_str == lang_def::keywordToStr(lang_def::Keyword::And))
			    ? builder.CreateAnd(lhs.value, rhs.value, "andtmp")
			    : builder.CreateOr(lhs.value, rhs.value, "ortmp");
			result = LoweredExpr{ .value = out, .type_id = ctx.bool_type_id };
			return;
		}

		if (lhs.type_id != ctx.i64_type_id || rhs.type_id != ctx.i64_type_id) {
			throw std::runtime_error(
				"Binary arithmetic operator currently only supports i64 operands"
			);
		}

		llvm::Value* out_value = nullptr;
		switch (op->getOperator().asNamed()) {
			case lang_def::NamedOperator::Plus:
				out_value = builder.CreateAdd(lhs.value, rhs.value, "addtmp");
				break;
			case lang_def::NamedOperator::Minus:
				out_value = builder.CreateSub(lhs.value, rhs.value, "subtmp");
				break;
			case lang_def::NamedOperator::Multiply:
				out_value = builder.CreateMul(lhs.value, rhs.value, "multmp");
				break;
			case lang_def::NamedOperator::Divide:
				// Signed division — i64 is signed in our type system.
				out_value = builder.CreateSDiv(lhs.value, rhs.value, "divtmp");
				break;
			default:
				throw std::runtime_error(
					"Binary operator '" + op->getOperator().value.str()
					+ "' is not yet supported (M5: + - * /)"
				);
		}

		result = LoweredExpr{ .value = out_value, .type_id = ctx.i64_type_id };
	}

	void ExprLowering::visitComparisonChain(pst::Access<pst::expr::ComparisonChain> chain) {
		// Chain `a OP1 b OP2 c` is lowered as `(a OP1 b) and (b OP2 c)`. Each link compares
		// adjacent values on i64 (only supported numeric type for now); result is bool.
		const auto& ops = chain->getOperators();
		if (ops.empty()) {
			throw std::runtime_error("Empty comparison chain (PST invariant violation)");
		}

		auto cmp_for = [&](lang_def::NamedOperator op_kind, llvm::Value* l, llvm::Value* r
		              ) -> llvm::Value* {
			switch (op_kind) {
				case lang_def::NamedOperator::Equal:    return builder.CreateICmpEQ(l, r, "eqtmp");
				case lang_def::NamedOperator::NotEqual: return builder.CreateICmpNE(l, r, "netmp");
				case lang_def::NamedOperator::Lesser:   return builder.CreateICmpSLT(l, r, "lttmp");
				case lang_def::NamedOperator::LEqual:   return builder.CreateICmpSLE(l, r, "letmp");
				case lang_def::NamedOperator::Greater:  return builder.CreateICmpSGT(l, r, "gttmp");
				case lang_def::NamedOperator::GEqual:   return builder.CreateICmpSGE(l, r, "getmp");
				default:
					throw std::runtime_error(
						"Unsupported comparison operator in chain (M6: == != < <= > >=)"
					);
			}
		};

		LoweredExpr  left      = lower(*chain->getSubExpr(0).illegalAccess().value());
		llvm::Value* combined  = nullptr;

		for (u64 i = 0; i < ops.size(); ++i) {
			LoweredExpr right = lower(*chain->getSubExpr(i + 1).illegalAccess().value());

			if (left.type_id != ctx.i64_type_id || right.type_id != ctx.i64_type_id) {
				throw std::runtime_error("Comparison currently only supports i64 operands");
			}

			llvm::Value* link = cmp_for(ops[i].asNamed(), left.value, right.value);
			combined          = combined ? builder.CreateAnd(combined, link, "chaintmp") : link;
			left              = right;
		}

		result = LoweredExpr{ .value = combined, .type_id = ctx.bool_type_id };
	}

	void ExprLowering::visitRoundExpr(pst::Access<pst::expr::RoundExpr> round) {
		// Parentheses carry no semantic meaning; just lower the inner expression.
		result = lower(*round->getInner().illegalAccess().value());
	}
}
