#include "builtin_operations.hpp"

#include <typesystem/higher/types.hpp>

#include <lang_definitions/key_spec_op.hpp>

#include <tuple>
#include <utility>

namespace {
	using namespace compiler;
	using namespace compiler::helios;
	using namespace compiler::helios::code;

	/**
	 * @brief Tries to find a common type for binary operation arguments through implicit coercion.
	 * @return Optional pair of (common_type, {left_coercion, right_coercion}).
	 */
	base::Optional<std::tuple<tsh::SymbolType<>, Coercion, Coercion>> findCommonTypewithCoercion(
		query::Context& ctx, base::CRef<Expr> lhs, base::CRef<Expr> rhs
	) {
		auto lhs_type = lhs->expression_type.getSymbolType();
		auto rhs_type = rhs->expression_type.getSymbolType();

		// Types the same -> no coercion.
		if (lhs_type.getType() == rhs_type.getType()) {
			return std::make_tuple(
				lhs_type, Coercion::emptyCoercion(lhs_type), Coercion::emptyCoercion(rhs_type)
			);
		}

		// Try coercing left to right.
		auto lhs_to_rhs = canCoerce(ctx, lhs_type, rhs_type);
		if (lhs_to_rhs.hasValueByType<Coercion>()) {
			return std::make_tuple(
				rhs_type,
				std::move(lhs_to_rhs.getValueByTypeOrPanic<Coercion>()),
				Coercion::emptyCoercion(rhs_type)
			);
		}

		// Try coercing right to left.
		auto rhs_to_lhs = canCoerce(ctx, rhs_type, lhs_type);
		if (rhs_to_lhs.hasValueByType<Coercion>()) {
			return std::make_tuple(
				lhs_type,
				Coercion::emptyCoercion(lhs_type),
				std::move(rhs_to_lhs.getValueByTypeOrPanic<Coercion>())
			);
		}

		// Invalid coercion.
		return {};
	}
}

namespace compiler::helios::code {
	base::Optional<std::tuple<BuiltinBinary, Coercion, Coercion>> findBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	) {
		auto common_type_res = findCommonTypewithCoercion(ctx, lhs, rhs);
		if (!common_type_res.has_value()) {
			// @TODO: report an error?
			return {};
		}

		auto& [common_type, lhs_coercion, rhs_coercion] = common_type_res.value();

		auto operation_kind = common_type.getType().getKind();


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

		if (operators.contains({ op, operation_kind }))
			return std::make_tuple(
				operators.at({ op, operation_kind }),
				std::move(lhs_coercion),
				std::move(rhs_coercion)
			);

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
