#include "type_name_extractor.hpp"

#include "const_eval.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/chain_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

#include <base/collections/optional.hpp>

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

	/// The single expression carried by a `[...]` link of an array type expression;
	/// throws when the link has anything but exactly one positional argument.
	const pst::ExprElement& dimensionExpr(
		pst::Access<pst::expr::Call> call, const std::string& what
	) {
		auto args = call->getArgs().illegalAccess().value();
		if (args->size() != 1)
			throw std::runtime_error(what + ": array type takes exactly one size per dimension");
		pst::Access<pst::CallArgument> arg = (*args->begin()).illegalAccess().value();
		if (arg->isNamedArg())
			throw std::runtime_error(what + ": array size cannot be a named argument");
		return *arg->getArg().illegalAccess().value()->getExpr().illegalAccess().value();
	}

	/// Attempts compile-time evaluation; empty when the expression is not a
	/// constant (a runtime dimension) rather than malformed.
	base::Optional<pg::ConstValue> tryConstEval(pg::Context& ctx, const pst::ExprElement& expr) {
		try {
			return pg::ConstEval(ctx).eval(expr);
		} catch (const std::exception&) { return {}; }
	}

	u64 validatedDimension(pg::Context& ctx, const pg::ConstValue& value, const std::string& what) {
		if (value.type_id != ctx.i64_type_id)
			throw std::runtime_error(what + ": array size must be an i64 constant");
		const auto size = i64(value.bits);
		if (size <= 0)
			throw std::runtime_error(
				what + ": array size must be positive, got " + std::to_string(size)
			);
		return u64(size);
	}

	u64 constantDimension(pg::Context& ctx, const pst::ExprElement& expr, const std::string& what) {
		const base::Optional<pg::ConstValue> value = tryConstEval(ctx, expr);
		if (!value.has_value())
			throw std::runtime_error(what + ": array size must be a compile-time constant");
		return validatedDimension(ctx, value.value(), what);
	}
}

namespace pg {
	base::StrID extractTypeName(const pst::ExprElement& expr) {
		TypeNameExtractor extractor;
		expr.acceptExprVisitor(extractor);
		return extractor.name;
	}

	ResolvedType resolveTypeExpr(
		Context& ctx, const pst::ExprElement& expr, const std::string& what, bool allow_vla
	) {
		// The common case: a bare identifier or type keyword.
		if (const base::StrID name = extractTypeName(expr); name != base::StrID())
			return ResolvedType{ .type_id = ctx.lookupTypeInScopeAndParents(ctx.currNs().id, name) };

		// Array type `T[N]...[M]`: a chain whose atom names the element type and whose
		// links are all square-bracket calls.
		const auto* chain = dynamic_cast<const pst::expr::ChainExpr*>(&expr);
		if (chain == nullptr) {
			throw std::runtime_error(
				what + ": type expression of kind '" + expr.elementType()
				+ "' is not a bare identifier or array type (not yet supported)"
			);
		}

		const base::StrID element_name = extractTypeName(*chain->getAtom().illegalAccess().value());
		if (element_name == base::StrID()) {
			throw std::runtime_error(
				what + ": array element type must be a bare identifier (not yet supported)"
			);
		}
		const tp::TypeID element_id
			= ctx.lookupTypeInScopeAndParents(ctx.currNs().id, element_name);

		std::vector<const pst::ExprElement*> dims;
		for (pst::AccessLocked<pst::ExprElement> link_locked: chain->getChain()) {
			pst::Access<pst::ExprElement> link = link_locked.illegalAccess().value();
			auto                          call = link.dynamicCast<pst::expr::Call>();
			if (!call.has_value() || call.value()->getType() != lexer::Token::BracketType::Square) {
				throw std::runtime_error(
					what + ": type expression of kind '" + expr.elementType()
					+ "' is not a bare identifier or array type (not yet supported)"
				);
			}
			dims.push_back(&dimensionExpr(call.value(), what));
		}
		CORE_ASSERT(!dims.empty(), "Chain type expression with no links");

		// Fold inner dimensions right-to-left so `i64[3][4]` reads like C's
		// `int a[3][4]`: 3 rows of `i64[4]`. Inner dimensions must be constant.
		tp::TypeID type_id = element_id;
		for (usize i = dims.size(); i-- > 1;)
			type_id = ctx.arrayTypeOf(type_id, constantDimension(ctx, *dims[i], what));

		// The outermost dimension may be a runtime expression when a VLA is allowed;
		// a constant that evaluates but is invalid (wrong type, non-positive) is
		// still an error, never a VLA.
		const base::Optional<ConstValue> outer = tryConstEval(ctx, *dims[0]);
		if (outer.has_value())
			return ResolvedType{
				.type_id = ctx.arrayTypeOf(type_id, validatedDimension(ctx, outer.value(), what))
			};
		if (!allow_vla)
			throw std::runtime_error(what + ": array size must be a compile-time constant");
		return ResolvedType{ .type_id = ctx.arrayTypeOf(type_id, 0), .vla_count = dims[0] };
	}
}
