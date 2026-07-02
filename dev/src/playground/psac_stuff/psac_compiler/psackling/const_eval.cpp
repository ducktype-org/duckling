#include "const_eval.hpp"

#include "char_literal.hpp"
#include "type_name_extractor.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/const.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace pg {
	ConstValue ConstEval::eval(const pst::ExprElement& expr) {
		result = {};
		expr.acceptExprVisitor(*this);
		return result;
	}

	void ConstEval::visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> lit) {
		result = ConstValue{
			.bits    = std::stoull(lit->getValue().str()),
			.type_id = ctx.i64_type_id,
		};
	}

	void ConstEval::visitExprCharValue(pst::Access<pst::expr::ExprCharValue> lit) {
		result = ConstValue{
			.bits    = static_cast<unsigned char>(charLiteralValue(lit)),
			.type_id = ctx.char_type_id,
		};
	}

	void ConstEval::visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) {
		switch (kw->getKeyword().illegalAccess().value()->unwrap()) {
		case lang_def::Keyword::True:
			result = ConstValue{ .bits = 1, .type_id = ctx.bool_type_id };
			return;
		case lang_def::Keyword::False:
			result = ConstValue{ .bits = 0, .type_id = ctx.bool_type_id };
			return;
		default:
			throw std::runtime_error(
				"Keyword '"
				+ lang_def::keywordToStr(kw->getKeyword().illegalAccess().value()->unwrap()).str()
				+ "' is not a valid constant expression"
			);
		}
	}

	void ConstEval::visitRoundExpr(pst::Access<pst::expr::RoundExpr> round) {
		result = eval(*round->getInner().illegalAccess().value());
	}

	void ConstEval::visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> op) {
		const ConstValue lhs = eval(*op->getLeftOperand().illegalAccess().value());
		const ConstValue rhs = eval(*op->getRightOperand().illegalAccess().value());

		// Logical and/or are keyword-operators; match by raw StrID (same trick as ExprLowering).
		const base::StrID op_str = op->getOperator().illegalAccess().value()->unwrap();
		if (op_str == lang_def::keywordToStr(lang_def::Keyword::And)
		    || op_str == lang_def::keywordToStr(lang_def::Keyword::Or)) {
			if (lhs.type_id != ctx.bool_type_id || rhs.type_id != ctx.bool_type_id)
				throw std::runtime_error(
					"Constant logical '" + op_str.str() + "' requires bool operands"
				);
			const bool out = (op_str == lang_def::keywordToStr(lang_def::Keyword::And))
			                   ? (lhs.bits != 0 && rhs.bits != 0)
			                   : (lhs.bits != 0 || rhs.bits != 0);
			result = ConstValue{ .bits = out ? u64(1) : u64(0), .type_id = ctx.bool_type_id };
			return;
		}

		if (lhs.type_id != ctx.i64_type_id || rhs.type_id != ctx.i64_type_id)
			throw std::runtime_error("Constant binary operator currently only supports i64 operands"
			);

		const i64 l = static_cast<i64>(lhs.bits);
		const i64 r = static_cast<i64>(rhs.bits);

		auto as_i64 = [&](i64 v) {
			return ConstValue{ .bits = static_cast<u64>(v), .type_id = ctx.i64_type_id };
		};
		auto as_bool = [&](bool v) {
			return ConstValue{ .bits = v ? u64(1) : u64(0), .type_id = ctx.bool_type_id };
		};

		switch (op->getOperator().illegalAccess().value()->unwrap().asNamed()) {
		case lang_def::NamedOperator::Plus:
			result = as_i64(l + r);
			return;
		case lang_def::NamedOperator::Minus:
			result = as_i64(l - r);
			return;
		case lang_def::NamedOperator::Multiply:
			result = as_i64(l * r);
			return;
		case lang_def::NamedOperator::Divide:
			if (r == 0) throw std::runtime_error("Division by zero in constant expression");
			result = as_i64(l / r);
			return;
		case lang_def::NamedOperator::Remainder:
			if (r == 0) throw std::runtime_error("Division by zero in constant expression");
			result = as_i64(l % r);
			return;
		case lang_def::NamedOperator::Equal:
			result = as_bool(l == r);
			return;
		case lang_def::NamedOperator::NotEqual:
			result = as_bool(l != r);
			return;
		case lang_def::NamedOperator::Lesser:
			result = as_bool(l < r);
			return;
		case lang_def::NamedOperator::LEqual:
			result = as_bool(l <= r);
			return;
		case lang_def::NamedOperator::Greater:
			result = as_bool(l > r);
			return;
		case lang_def::NamedOperator::GEqual:
			result = as_bool(l >= r);
			return;
		default:
			throw std::runtime_error(
				"Binary operator '" + op_str.str() + "' is not supported in constant expressions"
			);
		}
	}

	void ConstEval::visitComparisonChain(pst::Access<pst::expr::ComparisonChain> chain) {
		const auto expr_count = usize(chain->numberOfSubExpressions());
		if (expr_count < 2)
			throw std::runtime_error("Empty comparison chain (PST invariant violation)");

		auto cmp = [](lang_def::NamedOperator op_kind, i64 l, i64 r) -> bool {
			switch (op_kind) {
			case lang_def::NamedOperator::Equal:
				return l == r;
			case lang_def::NamedOperator::NotEqual:
				return l != r;
			case lang_def::NamedOperator::Lesser:
				return l < r;
			case lang_def::NamedOperator::LEqual:
				return l <= r;
			case lang_def::NamedOperator::Greater:
				return l > r;
			case lang_def::NamedOperator::GEqual:
				return l >= r;
			default:
				throw std::runtime_error(
					"Unsupported comparison operator in constant chain (== != < <= > >=)"
				);
			}
		};

		ConstValue left     = eval(*chain->getSubExpr(0).illegalAccess().value());
		bool       combined = true;

		for (u64 i = 0; i + 1 < expr_count; ++i) {
			ConstValue right = eval(*chain->getSubExpr(i + 1).illegalAccess().value());

			if (left.type_id != ctx.i64_type_id || right.type_id != ctx.i64_type_id)
				throw std::runtime_error("Constant comparison currently only supports i64 operands");

			combined = combined
			        && cmp(chain->getOperator(i).illegalAccess().value()->unwrap().asNamed(),
			               static_cast<i64>(left.bits),
			               static_cast<i64>(right.bits));
			left = right;
		}

		result = ConstValue{ .bits = combined ? u64(1) : u64(0), .type_id = ctx.bool_type_id };
	}

	ConstValue ConstEval::constantFromSymbol(sym::SymbolID sym_id, base::StrID name) {
		const sym::Symbol& symbol = ctx.getSymbol(sym_id);
		if (!std::holds_alternative<sym::ConstantSymbol>(symbol.kind)) {
			throw std::runtime_error(
				"Identifier '" + name.str() + "' is not a constant; only constants may appear in "
				"constant expressions"
			);
		}
		return ConstValue{ .bits    = std::get<sym::ConstantSymbol>(symbol.kind).bits,
			               .type_id = symbol.type_id };
	}

	void ConstEval::visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) {
		const base::StrID name = ident->getName().illegalAccess().value()->unwrap();
		result = constantFromSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name), name);
	}

	void ConstEval::visitChainExpr(pst::Access<pst::expr::ChainExpr> chain) {
		// Only namespace-qualified constant access (`ns.CONST`) is supported here.
		pst::Access<pst::ExprElement> atom = chain->getAtom().illegalAccess().value();

		auto ident_opt = atom.dynamicCast<pst::expr::IdentifierLiteral>();
		if (!ident_opt.has_value())
			throw std::runtime_error("Constant chain expression must start with an identifier");

		base::StrID   name   = ident_opt.value()->getName().illegalAccess().value()->unwrap();
		sym::SymbolID sym_id = ctx.lookupInScopeAndParents(ctx.currNs().id, name);

		for (pst::AccessLocked<pst::ExprElement> element_locked: chain->getChain()) {
			pst::Access<pst::ExprElement> element = element_locked.illegalAccess().value();

			auto access_opt = element.dynamicCast<pst::expr::Access>();
			if (!access_opt.has_value()) {
				throw std::runtime_error(
					"Constant chain expression: only `.` namespace access is supported"
				);
			}

			const sym::Symbol& symbol = ctx.getSymbol(sym_id);
			if (!std::holds_alternative<sym::NamespaceSymbol>(symbol.kind)) {
				throw std::runtime_error(
					"'" + name.str() + "' is not a namespace in constant expression"
				);
			}
			const ns::NamespaceID ns_id = std::get<sym::NamespaceSymbol>(symbol.kind).namespace_id;

			name   = access_opt.value()->getName().illegalAccess().value()->unwrap();
			sym_id = ctx.lookupInNamespace(ns_id, name);
		}

		result = constantFromSymbol(sym_id, name);
	}

	void registerConstSymbol(Context& ctx, pst::Access<pst::Const> stmt) {
		const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();

		if (!stmt->getValue().has_value())
			throw std::runtime_error("Constant '" + name.str() + "' must have an initializer");

		const pst::ExprElement& value_expr
			= *stmt->getValue()->illegalAccess().value()->getExpr().illegalAccess().value();

		ConstEval  evaluator(ctx);
		ConstValue value = evaluator.eval(value_expr);

		if (stmt->getType().has_value()) {
			const pst::ExprElement& type_expr
				= *stmt->getType()->illegalAccess().value()->getExpr().illegalAccess().value();
			const base::StrID type_name = extractTypeName(type_expr);
			if (type_name == base::StrID()) {
				throw std::runtime_error(
					"Constant '" + name.str() + "': type expression of kind '"
					+ type_expr.elementType() + "' is not a bare identifier (not yet supported)"
				);
			}
			tp::TypeID declared_type = ctx.lookupTypeInScopeAndParents(ctx.currNs().id, type_name);
			if (declared_type != value.type_id) {
				throw std::runtime_error(
					"Constant '" + name.str() + "': initializer type does not match declared type '"
					+ type_name.str() + "'"
				);
			}
		}

		ctx.insertSymbol(name, sym::ConstantSymbol{ .bits = value.bits }, value.type_id);
	}
}
