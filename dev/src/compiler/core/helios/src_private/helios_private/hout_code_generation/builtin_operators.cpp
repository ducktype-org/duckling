#include "builtin_operators.hpp"

#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

namespace compiler::helios::houtgen {
	Box<code::Expr> generateBuiltinOperatorExpression(
		query::Context&                              ctx,
		SymID                                        operator_symbol,
		std::vector<Box<code::Expr>>                 arguments,
		const base::Optional<std::vector<Coercion>>& coercions
	) {
		const auto op_name = name(operator_symbol);

		auto       lhs        = coercions->at(0).coerce(ctx, std::move(arguments.at(0)));
		auto       rhs        = coercions->at(1).coerce(ctx, std::move(arguments.at(1)));
		const auto left_kind  = lhs->expression_type.getType().getKind();
		const auto right_kind = rhs->expression_type.getType().getKind();
		using enum tsh::Kind;

		/// Integer modulo ///
		if (op_name == base::StrID("%") && left_kind == Integral && right_kind == Integral) {
			return makeBox<code::BinaryOperatorExpr>(
				ctx, code::BuiltinBinary::IntegerMod, std::move(lhs), std::move(rhs)
			);
		}

		/// Meta comparison ///
		if (left_kind == Meta && right_kind == Meta) {
			static const base::Map<base::StrID, code::BuiltinBinary> name_to_op{
				{ base::StrID("=="), code::BuiltinBinary::MetaEq },
				{ base::StrID("!="), code::BuiltinBinary::MetaNeq },
			};
			return makeBox<code::BinaryOperatorExpr>(
				ctx, name_to_op.at(op_name), std::move(lhs), std::move(rhs)
			);
		}

		/// Boolean operations ///
		if (left_kind == Bool && right_kind == Bool) {
			static const base::Map<base::StrID, code::BuiltinBinary> name_to_op{
				{ keywordToStr(lang_def::Keyword::And), code::BuiltinBinary::BooleanAnd },
				{ keywordToStr(lang_def::Keyword::Or), code::BuiltinBinary::BooleanOr },
				{ base::StrID("=="), code::BuiltinBinary::IntegerEq },
				{ base::StrID("!="), code::BuiltinBinary::IntegerNeq },
			};
			return makeBox<code::BinaryOperatorExpr>(
				ctx, name_to_op.at(op_name), std::move(lhs), std::move(rhs)
			);
		}

		/// Character arithmetic ///
		base::Map<base::StrID, code::BuiltinBinary> comparisons{
			{ base::StrID("<"), code::BuiltinBinary::IntegerLt },
			{ base::StrID("<="), code::BuiltinBinary::IntegerLteq },
			{ base::StrID(">"), code::BuiltinBinary::IntegerGt },
			{ base::StrID(">="), code::BuiltinBinary::IntegerGteq },
			{ base::StrID("=="), code::BuiltinBinary::IntegerEq },
			{ base::StrID("!="), code::BuiltinBinary::IntegerNeq },
		};
		if (comparisons.contains(op_name) && left_kind == Char && right_kind == Char) {
			return makeBox<code::BinaryOperatorExpr>(
				ctx, comparisons.at(op_name), std::move(lhs), std::move(rhs)
			);
		}
		if (op_name == base::StrID("-") && left_kind == Char && right_kind == Char) {
			return makeBox<code::BinaryOperatorExpr>(
				ctx, code::BuiltinBinary::IntegerSub, std::move(lhs), std::move(rhs)
			);
		}
		if (op_name == base::StrID("+") && left_kind == Integral && right_kind == Char) {
			return makeBox<code::BinaryOperatorExpr>(
				ctx, code::BuiltinBinary::IntegerAdd, std::move(lhs), std::move(rhs)
			);
		}
		if (op_name == base::StrID("+") && left_kind == Char && right_kind == Integral) {
			return makeBox<code::BinaryOperatorExpr>(
				ctx, code::BuiltinBinary::IntegerAdd, std::move(lhs), std::move(rhs)
			);
		}

		// Implementation for builtin operator not found.
		CORE_UNREACHABLE();
	}
}
