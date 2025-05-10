
#include "builtins.hpp"

#include <lang_definitions/key_spec_op.hpp>
#include <typesystem/higher/queries.hpp>

namespace compiler::helios::code {

	base::Optional<BuiltinBinary> findBinaryBuiltin(lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs) {
		// note: this is mock that works only for very simple int op int and bool op bool.
		// @todo: make it smarter?
		// when refactoring it remember about unaryBuiltin

		// Get argument types.
		auto lhs_type = lhs->expression_type;
		auto rhs_type = rhs->expression_type;

		// Confirm appropriate types.
		auto argument_kind           = lhs_type.getType().getKind();
		bool are_arguments_same_kind = argument_kind == rhs_type.getType().getKind();
		bool are_arguments_int_or_bool
			= argument_kind == tsh::Kind::Integral or argument_kind == tsh::Kind::Bool;

		if (not are_arguments_same_kind or not are_arguments_int_or_bool) {
			// @TODO: report an error?
			// No builtins for types other than ints and bools for now.
			return {};
		}

		// Confirm matching sizes and signedness in the case of integers.
		if (argument_kind == tsh::Kind::Integral) {
			auto lhs_as_integer = tsh::IntegralAbstractType(lhs_type.getType());
			auto rhs_as_integer = tsh::IntegralAbstractType(rhs_type.getType());

			if (lhs_as_integer.getSize() != rhs_as_integer.getSize()
			    or lhs_as_integer.getSignedness() != rhs_as_integer.getSignedness()) {
				return {};
			}
		}

		// @TODO: change to base::map when possible
		const static std::map<base::StrID, BuiltinBinary> operators = {
			{ base::StrID("+"), BuiltinBinary::IntegerAdd },
			{ base::StrID("-"), BuiltinBinary::IntegerSub },
			{ base::StrID("*"), BuiltinBinary::IntegerMul },
			{ base::StrID("/"), BuiltinBinary::IntegerDiv },
			{ base::StrID("%"), BuiltinBinary::IntegerMod },
			{ base::StrID("**"), BuiltinBinary::IntegerPow },
			{ base::StrID("<"), BuiltinBinary::IntegerLt },
			{ keywordToStr(lang_def::Keyword::And), BuiltinBinary::BooleanAnd },
			{ keywordToStr(lang_def::Keyword::Or), BuiltinBinary::BooleanOr },
		};

		if (operators.contains(op)) return operators.at(op);
		return {};
	}

	base::Optional<BuiltinUnary> findUnaryBuiltin(lexer::Operator op, CRef<Expr> expr) {
		// note: this is mock that works only for very simple int, bool operations.
		// when refactoring it remember about binaryBuiltin

		auto expr_type = expr->expression_type;

		if (expr_type.getType().getKind() != tsh::Kind::Integral
		    and expr_type.getType().getKind() != tsh::Kind::Bool) {
			// No builtins for this case for types other than ints and bools for now.
			return {};
		}

		// @TODO: change to base::map when possible
		const static std::map<base::StrID, BuiltinUnary> operators = {
			{ base::StrID("-"), BuiltinUnary::IntegerNegation },
			{ keywordToStr(lang_def::Keyword::Not), BuiltinUnary::BooleanNot },
		};

		if (operators.contains(op)) return operators.at(op);

		return {};
	}
}
