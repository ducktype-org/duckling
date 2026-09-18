#include "passing.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {
	PassingMethod passingMethod(query::Context& ctx, const tsh::ExpressionType<>& value) {
		const tsh::SymbolType<> symbol_type = value.getSymbolType();

		// If the value is being moved already, we pass it by `ByteCopy`.
		if (value.getValueCategory().mustMove()) return PassingMethod::ByteCopy;

		switch (value.getValueCategory().getCategory()) {
		case tsh::PrimaryCategory::Temporary:
			// This is a purely an optimization,
			// which keeps the generated HOUT free of `implicit_move` on everything.
			// A trivially destructible one owns nothing to hand over,
			// and moving it out of would produce exactly the same code as copying its bytes.
			if (symbol_type.isTriviallyDestructible(ctx) and symbol_type.isTriviallyCopyable(ctx))
				return PassingMethod::ByteCopy;
			if (value.getValueCategory().isMovableFrom()) return PassingMethod::ImplicitMove;
			return PassingMethod::ExplicitCopyOrMove;
		case tsh::PrimaryCategory::Literal:
			return PassingMethod::ByteCopy;
		case tsh::PrimaryCategory::Local:
		case tsh::PrimaryCategory::Global:
		case tsh::PrimaryCategory::Dereferenced:
			if (symbol_type.isTriviallyCopyable(ctx)) return PassingMethod::ByteCopy;
			return symbol_type.isCopyable(ctx) ? PassingMethod::ExplicitCopyOrMove
			                                   : PassingMethod::NotCopyable;
		}
		CORE_UNREACHABLE();
	}

	Box<code::Expr> moveReturnedLocal(query::Context& ctx, Box<code::Expr> value) {
		const tsh::ExpressionType<> type = value->expression_type;

		if (type.getValueCategory().getCategory() == tsh::PrimaryCategory::Local) {
			// A projection of a local is not movable from, for example.
			if (not type.getValueCategory().isMovableFrom()) return value;
			auto origin = value->origin.generatedFrom();
			return makeBox<code::MoveExpr>(
				ctx, origin, std::move(value), code::MoveExpr::MoveKind::Implicit
			);
		}
		return value;
	}

	bool requiresValueCopy(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind) {
		switch (from_kind) {
		case tsh::ReferenceKind::Direct:
		case tsh::ReferenceKind::Ref:
		case tsh::ReferenceKind::Box:
			return to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box;
		}
		return false;
	}

	bool readsThroughReference(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind) {
		return (from_kind == tsh::ReferenceKind::Ref
		        && (to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box))
		    || (from_kind == tsh::ReferenceKind::Box && to_kind == tsh::ReferenceKind::Direct);
	}

	tsh::ExpressionType<> valueBeingCopied(
		const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	) {
		const tsh::SymbolType<> from_type = from.getSymbolType();

		if (!readsThroughReference(from_type.getRefKind(), to.getRefKind())) return from;
		return { from_type.getPointeeSymbolType(),
			     tsh::ValueCategory(tsh::PrimaryCategory::Dereferenced) };
	}
}
