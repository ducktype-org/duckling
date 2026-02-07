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
	 * @brief Tries to find a common type for builtin binary operation arguments through implicit
	 * coercion. If any of the arguments is not a direct type a coercion to the direct type will be
	 * forced.
	 * @return Optional pair of (common_type, {left_coercion, right_coercion}).
	 *
	 * @TODO: #973 this function panics on query::Failed in coercions, should probably propagate
	 * failed instead. If we conclude, that this will return empty optional on failure, we should
	 * document it here.
	 */
	base::Optional<std::tuple<tsh::SymbolType<>, Coercion, Coercion>> findCommonTypeWithCoercion(
		query::Context& ctx, base::CRef<Expr> lhs, base::CRef<Expr> rhs
	) {
		auto lhs_type = lhs->expression_type.getSymbolType();
		auto rhs_type = rhs->expression_type.getSymbolType();

		// Builtin operators only work on direct values. When provided with references or box types
		// we have to force a coercion to a direct type which will insert a DerefExpr. This is
		// needed to handle cases like: var x = referenceA + referenceB.
		auto lhs_direct = lhs_type.withReferenceKind(tsh::ReferenceKind::Direct);
		auto rhs_direct = rhs_type.withReferenceKind(tsh::ReferenceKind::Direct);

		// Try to coerce both values to the rhs direct type.
		auto lhs_to_rhs = canCoerce(ctx, lhs_type, rhs_direct);
		auto rhs_to_rhs = canCoerce(ctx, rhs_type, rhs_direct);

		if (lhs_to_rhs.valueOrThrow().isValid() && rhs_to_rhs.valueOrThrow().isValid()) {
			return std::make_tuple(
				rhs_direct,
				std::move(lhs_to_rhs.valueOrThrow()).getCoercion(),
				std::move(rhs_to_rhs.valueOrThrow()).getCoercion()
			);
		}

		// Try to coerce both values to the lhs direct type.
		auto lhs_to_lhs = canCoerce(ctx, lhs_type, lhs_direct);
		auto rhs_to_lhs = canCoerce(ctx, rhs_type, lhs_direct);

		if (lhs_to_lhs.valueOrThrow().isValid() && rhs_to_lhs.valueOrThrow().isValid()) {
			return std::make_tuple(
				lhs_direct,
				std::move(lhs_to_lhs.valueOrThrow()).getCoercion(),
				std::move(rhs_to_lhs.valueOrThrow()).getCoercion()
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
		auto common_type_res = findCommonTypeWithCoercion(ctx, lhs, rhs);
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

			/// Meta type comparisons ///
			{ { base::StrID("=="), tsh::Kind::Meta }, BuiltinBinary::MetaEq },
			{ { base::StrID("!="), tsh::Kind::Meta }, BuiltinBinary::MetaNeq },

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

	base::Optional<std::tuple<BuiltinUnary, Coercion>> findUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	) {
		auto source_type = expr->expression_type.getSymbolType();

		Coercion unary_coercion = [&]() -> Coercion {
			// If the operation operates on Direct values we need to perform a
			// coercion from a ref / box type the direct type. This is needed to handle cases
			// like: var x: i32 = -someReference.
			if (source_type.getRefKind() != tsh::ReferenceKind::Direct) {
				auto direct_type = source_type.withReferenceKind(tsh::ReferenceKind::Direct);
				auto res         = canCoerce(ctx, source_type, direct_type);
				if (res.valueOrThrow().isValid())
					return std::move(res.valueOrThrow()).getCoercion();
			}

			// By default the coercion for unary builtins is empty.
			return Coercion::emptyCoercion(source_type);
		}();

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
			// List. TODOP: Temporary
			{ { keywordToStr(lang_def::Keyword::Len), tsh::Kind::DynamicArray }, BuiltinUnary::Len },
		};

		// Single lookup
		auto it = lookup.find({ op, kind });
		if (it != lookup.end()) return std::make_tuple(it->second, std::move(unary_coercion));

		return {};  // Not found
	}
}
