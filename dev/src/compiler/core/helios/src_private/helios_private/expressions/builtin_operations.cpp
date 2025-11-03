
#include "builtin_operations.hpp"

#include "typesystem/higher/kind.hpp"

#include <typesystem/higher/types.hpp>

#include "base/str/str_utils.hpp"

#include <lang_definitions/key_spec_op.hpp>

#include <iostream>


#define DEBUG(CONTENT) std::cout << "[FIND BINARY BUILTIN]: " << CONTENT << '\n';

namespace compiler::helios::code {

	base::Optional<BuiltinBinary> findBinaryBuiltin(
		lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	) {
		// note: this is mock that works only for very simple int op int and bool op bool.
		// @todo: make it smarter?
		// when refactoring it remember about unaryBuiltin

		// TODOOOOOOO: This is broken!!!!
		DEBUG("Called");
		DEBUG("Lhs");
		lhs->debugPrint(std::cout);
		std::cout << '\n';
		DEBUG("Rhs");
		rhs->debugPrint(std::cout);
		std::cout << '\n';

		// Get argument types.
		auto lhs_type = lhs->expression_type;
		auto rhs_type = rhs->expression_type;

		// Confirm appropriate types.
		auto argument_kind = lhs_type.getType().getKind();
		DEBUG("LHS arg kind");
		DEBUG(base::enumToStr(argument_kind).strView());
		DEBUG("RHS arg kind");
		DEBUG(base::enumToStr(rhs_type.getType().getKind()).strView());
		bool are_arguments_same_kind         = argument_kind == rhs_type.getType().getKind();
		bool are_arguments_int_float_or_bool = argument_kind == tsh::Kind::Integral
		                                    or argument_kind == tsh::Kind::Bool
		                                    or argument_kind == tsh::Kind::Bool;

		if (not are_arguments_same_kind or not are_arguments_int_float_or_bool) {
			// @TODO: report an error?
			// No builtins for types other than ints, floats and bools for now.
			DEBUG("BINARY TYPE MISMATCH");
			return {};
		}

		// Confirm matching sizes and signedness in the case of integers.
		if (argument_kind == tsh::Kind::Integral) {
			auto lhs_as_integer = tsh::IntegralAbstractType(lhs_type.getType());
			auto rhs_as_integer = tsh::IntegralAbstractType(rhs_type.getType());

			DEBUG("LHS integral size");
			DEBUG(usize(lhs_as_integer.getSize()));
			DEBUG("RHS integral size");
			DEBUG(usize(rhs_as_integer.getSize()));

			// TODOP: Do sizes have to match?
			if (lhs_as_integer.getSize() != rhs_as_integer.getSize()
			    or lhs_as_integer.getSignedness() != rhs_as_integer.getSignedness()) {
				DEBUG("Integral: BINARY SIZE MISMATCH");
				return {};
			}
		} else if (argument_kind == tsh::Kind::Float) {
			auto lhs_as_integer = tsh::FloatAbstractType(lhs_type.getType());
			auto rhs_as_integer = tsh::FloatAbstractType(rhs_type.getType());

			DEBUG("LHS floating size");
			DEBUG(usize(lhs_as_integer.getSize()));
			DEBUG("RHS floating size");
			DEBUG(usize(rhs_as_integer.getSize()));

			// TODOP: Do sizes have to match?
			if (lhs_as_integer.getSize() != rhs_as_integer.getSize()) {
				DEBUG("Floating point: BINARY SIZE MISMATCH");
				return {};
			}
		}

		// @TODO: change to base::map when possible
		const static std::map<std::pair<lexer::Operator, tsh::Kind>, BuiltinBinary> operators = {
			/// Integer arithmetic ///
			{ { base::StrID("+"), tsh::Kind::Integral }, BuiltinBinary::IntegerAdd },
			{ { base::StrID("-"), tsh::Kind::Integral }, BuiltinBinary::IntegerSub },
			{ { base::StrID("*"), tsh::Kind::Integral }, BuiltinBinary::IntegerMul },
			{ { base::StrID("/"), tsh::Kind::Integral }, BuiltinBinary::IntegerDiv },
			{ { base::StrID("%"), tsh::Kind::Integral }, BuiltinBinary::IntegerMod },
			{ { base::StrID("**"), tsh::Kind::Integral }, BuiltinBinary::IntegerPow },

			/// Integer comparisons ///
			{ { base::StrID("<"), tsh::Kind::Integral }, BuiltinBinary::IntegerLt },
			{ { base::StrID(">"), tsh::Kind::Integral }, BuiltinBinary::IntegerGt },
			{ { base::StrID("<="), tsh::Kind::Integral }, BuiltinBinary::IntegerLteq },
			{ { base::StrID(">="), tsh::Kind::Integral }, BuiltinBinary::IntegerGteq },
			{ { base::StrID("=="), tsh::Kind::Integral }, BuiltinBinary::IntegerEq },
			{ { base::StrID("!="), tsh::Kind::Integral }, BuiltinBinary::IntegerNeq },

			/// Floating point arithmetic ///
			{ { base::StrID("+"), tsh::Kind::Float }, BuiltinBinary::FloatAdd },
			{ { base::StrID("-"), tsh::Kind::Float }, BuiltinBinary::FloatSub },
			{ { base::StrID("*"), tsh::Kind::Float }, BuiltinBinary::FloatMul },
			{ { base::StrID("/"), tsh::Kind::Float }, BuiltinBinary::FloatDiv },
			{ { base::StrID("**"), tsh::Kind::Float }, BuiltinBinary::FloatPow },

			/// Floating point comparisons ///
			{ { base::StrID("<"), tsh::Kind::Float }, BuiltinBinary::FloatLt },
			{ { base::StrID(">"), tsh::Kind::Float }, BuiltinBinary::FloatGt },
			{ { base::StrID("<="), tsh::Kind::Float }, BuiltinBinary::FloatLteq },
			{ { base::StrID(">="), tsh::Kind::Float }, BuiltinBinary::FloatGteq },
			{ { base::StrID("=="), tsh::Kind::Float }, BuiltinBinary::FloatEq },
			{ { base::StrID("!="), tsh::Kind::Float }, BuiltinBinary::FloatNeq },

			{ { keywordToStr(lang_def::Keyword::And), tsh::Kind::Bool }, BuiltinBinary::BooleanAnd },
			{ { keywordToStr(lang_def::Keyword::Or), tsh::Kind::Bool }, BuiltinBinary::BooleanOr },
		};

		if (operators.contains({ op, argument_kind })) return operators.at({ op, argument_kind });
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
			// Floating point
			{ { base::StrID("-"), tsh::Kind::Float }, BuiltinUnary::FloatNegation },
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
