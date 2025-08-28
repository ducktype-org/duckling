
#include "builtin_operations.hpp"

#include <typesystem/higher/queries.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace compiler::helios::code {

	base::Optional<BuiltinBinary> findBinaryBuiltin(
		lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	) {
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

	// Aliases to keep things concise
	using OpKindPair = std::pair<lexer::Operator, tsh::Kind>;
	using LookupMap  = std::map<OpKindPair, BuiltinUnary>;

	base::Optional<BuiltinUnary> findUnaryBuiltin(lexer::Operator op, CRef<Expr> expr) {
		auto kind = expr->expression_type.getType().getKind();

		// Initialize once
		static const LookupMap lookup = {
			// Integral
			{ { base::StrID("-"), tsh::Kind::Integral }, BuiltinUnary::IntegerNegation },
			// Boolean
			{ { keywordToStr(lang_def::Keyword::Not), tsh::Kind::Bool }, BuiltinUnary::BooleanNot },
			// Meta
			{ { keywordToStr(lang_def::Keyword::Ref), tsh::Kind::Meta }, BuiltinUnary::Ref },
			{ { keywordToStr(lang_def::Keyword::Box), tsh::Kind::Meta }, BuiltinUnary::Box },
			{ { keywordToStr(lang_def::Keyword::Const), tsh::Kind::Meta }, BuiltinUnary::Const },
		};

		// Single lookup
		auto it = lookup.find({ op, kind });
		if (it != lookup.end()) return it->second;

		return {};  // Not found
	}
}
