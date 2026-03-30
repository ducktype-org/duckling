#include "builtin_operators.hpp"

#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/maps.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/standard_query/query_impl.hpp>

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
	base::Optional<std::tuple<BuiltinBinary, Coercion, Coercion>> findNumericBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	) {
		auto common_type_res = findCommonTypeWithCoercion(ctx, lhs, rhs);
		if (!common_type_res.has_value()) return {};

		auto& [common_type, lhs_coercion, rhs_coercion] = common_type_res.value();

		auto operation_kind = common_type.getType().getKind();

		const static base::Map<std::pair<lexer::Operator, tsh::Kind>, BuiltinBinary> numeric_operators
			= {
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
				  { { base::StrID("%"), tsh::Kind::Float }, BuiltinBinary::FloatMod },
				  { { base::StrID("**"), tsh::Kind::Float }, BuiltinBinary::FloatPow },

				  /// Floating point comparisons ///
				  { { base::StrID("<"), tsh::Kind::Float }, BuiltinBinary::FloatLt },
				  { { base::StrID(">"), tsh::Kind::Float }, BuiltinBinary::FloatGt },
				  { { base::StrID("<="), tsh::Kind::Float }, BuiltinBinary::FloatLteq },
				  { { base::StrID(">="), tsh::Kind::Float }, BuiltinBinary::FloatGteq },
				  { { base::StrID("=="), tsh::Kind::Float }, BuiltinBinary::FloatEq },
				  { { base::StrID("!="), tsh::Kind::Float }, BuiltinBinary::FloatNeq },
			  };

		if (numeric_operators.contains({ op, operation_kind }))
			return std::make_tuple(
				numeric_operators.at({ op, operation_kind }),
				std::move(lhs_coercion),
				std::move(rhs_coercion)
			);

		return {};
	}

	struct IMPLEMENT_QUERY(QueryRegularBinaryBuiltinSymbols, RegularBinaryBuiltinSymbolMap) {
		static auto provide(Context& ctx, QKey) -> PResult {
			// Preamble
			const auto bool_t = tsh::SymbolType<>{
				tsh::getBoolType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			using enum BuiltinBinary;

			// The result map and a helper function to populate it.
			auto       result_ops = RegularBinaryBuiltinSymbolMap{};
			const auto builtin_op = [&ctx, &result_ops](
										const base::StrID              name,
										std::vector<tsh::SymbolType<>> param_types,
										const tsh::SymbolType<>        return_type,
										const BuiltinBinary            op
									) -> void {
				auto builtin = RegularBinaryBuiltin{
					.symbol = ctx.query<defgen::QueryGeneratedSymbol>({
						.name = name,
						.generated_symbol_data
						= defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::BuiltinOperator{
							ctx.query<tsh::QueryFunctionType>({
								std::move(param_types),
								return_type,
							}),
						} },
					}),
					.op     = op,
				};
				result_ops.put(builtin.symbol, builtin);
			};

			/// Meta comparisons ///
			const auto meta_t = tsh::SymbolType<>{
				tsh::getMetaType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			builtin_op(base::StrID("=="), { meta_t, meta_t }, bool_t, MetaEq);
			builtin_op(base::StrID("!="), { meta_t, meta_t }, bool_t, MetaNeq);

			/// Boolean operations ///
			const auto kw_and = keywordToStr(lang_def::Keyword::And);
			const auto kw_or  = keywordToStr(lang_def::Keyword::Or);
			builtin_op(kw_and, { bool_t, bool_t }, bool_t, BooleanAnd);
			builtin_op(kw_or, { bool_t, bool_t }, bool_t, BooleanOr);
			builtin_op(base::StrID("=="), { bool_t, bool_t }, bool_t, IntegerEq);
			builtin_op(base::StrID("!="), { bool_t, bool_t }, bool_t, IntegerNeq);

			/// Character arithmetic ///
			const auto u8_t = tsh::SymbolType<>{
				tsh::getIntegralType(ctx, 8, tsh::IntegralAbstractType::Signedness::Unsigned),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			const auto char_t = tsh::SymbolType<>{
				tsh::getCharType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			builtin_op(base::StrID("<"), { char_t, char_t }, bool_t, IntegerLt);
			builtin_op(base::StrID("<="), { char_t, char_t }, bool_t, IntegerLteq);
			builtin_op(base::StrID(">"), { char_t, char_t }, bool_t, IntegerGt);
			builtin_op(base::StrID(">="), { char_t, char_t }, bool_t, IntegerGteq);
			builtin_op(base::StrID("=="), { char_t, char_t }, bool_t, IntegerEq);
			builtin_op(base::StrID("!="), { char_t, char_t }, bool_t, IntegerNeq);
			builtin_op(base::StrID("-"), { char_t, char_t }, u8_t, IntegerSub);
			builtin_op(base::StrID("+"), { u8_t, char_t }, char_t, IntegerAdd);
			builtin_op(base::StrID("+"), { char_t, u8_t }, char_t, IntegerAdd);

			// Return
			return result_ops;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRegularBinaryBuiltinSymbols)

	// Aliases to keep things concise
	using OpKindPair = std::pair<lexer::Operator, tsh::Kind>;
	using LookupMap  = std::map<OpKindPair, BuiltinUnary>;

	base::Optional<std::tuple<BuiltinUnary, Coercion>> findUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	) {
		auto source_type = expr->expression_type.getSymbolType();

		Coercion unary_coercion = [&]() -> Coercion {
			bool is_len = (op == lang_def::keywordToStr(lang_def::Keyword::Len));
			// If the operation operates on Direct values we need to perform a
			// coercion from a ref / box type the direct type. This is needed to handle cases
			// like: var x: i32 = -someReference.
			if (source_type.getRefKind() != tsh::ReferenceKind::Direct) {
				auto direct_type = source_type.withReferenceKind(tsh::ReferenceKind::Direct);

				// @TODO: #1970 If the operator is `len` we have to bypass the trivial copyability
				// check for now. This is because the temporarily added `len` operator operates on
				// direct values thus any usage of it on reference types would need to perform a
				// deref (which means a copy, but copying lists is not yet implemented) thus
				// `canCoerce` returns an error.
				// Since `len` operator existence is temporary we mock it out and insert a deref
				// either way. This will copy the list struct, but not copy the heap data, but this
				// is acceptable in case of `len`. A more solid approach would be for the `len`
				// operator to take a reference to the list, but this would require more
				// architectural changes. `len` operator will be replaced by the `.length()`
				// method/field in the future, thus for release purposes is mocked up.
				auto res = canCoerce(ctx, source_type, direct_type, is_len);
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
			// List.
			{ { keywordToStr(lang_def::Keyword::Len), tsh::Kind::DynamicArray }, BuiltinUnary::Len },
		};

		// Single lookup
		auto it = lookup.find({ op, kind });
		if (it != lookup.end()) return std::make_tuple(it->second, std::move(unary_coercion));

		return {};  // Not found
	}
}
