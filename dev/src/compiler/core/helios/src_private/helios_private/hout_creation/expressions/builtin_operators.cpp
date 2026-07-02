#include "builtin_operators.hpp"

#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/maps.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include "crow/json.h"

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
	base::Optional<std::tuple<BuiltinUnary, Coercion>> findNumericUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> expr
	) {
		auto operation_kind = expr->expression_type.getType().getKind();

		const static base::Map<std::pair<lexer::Operator, tsh::Kind>, BuiltinUnary> numeric_operators
			= {
				  /// Negations ///
				  { { base::StrID("-"), tsh::Kind::Integral }, BuiltinUnary::IntegerNegation },
				  { { base::StrID("-"), tsh::Kind::Float }, BuiltinUnary::FloatNegation },
			  };

		if (numeric_operators.contains({ op, operation_kind }))
			return std::make_tuple(
				numeric_operators.at({ op, operation_kind }),
				Coercion::emptyCoercion(expr->expression_type.getSymbolType())
			);

		return {};
	}

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

	struct IMPLEMENT_QUERY(QueryRegularBuiltinOperatorSymbols, RegularBuiltinOperatorSymbolMap) {
		static auto provide(Context& ctx, QKey) -> PResult {
			// Preamble
			const auto bool_t = tsh::SymbolType<>{
				tsh::getBoolType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			using enum HOUTFunctionDeclaration::Operatoriness;
			using enum BuiltinUnary;
			using enum BuiltinBinary;
			using Keyword = lang_def::Keyword;

			// The result map and a helper functions to populate it.
			auto result_ops = RegularBuiltinOperatorSymbolMap{};
			// - Helper function to register a builtin operation that results in a BuiltinBinary.
			const auto builtin_op = [&ctx, &result_ops](
										const base::StrID                               name,
										std::vector<tsh::SymbolType<>>                  param_types,
										const tsh::SymbolType<>                         return_type,
										const std::variant<BuiltinUnary, BuiltinBinary> bop,
										const HOUTFunctionDeclaration::Operatoriness operatoriness
									) -> void {
				auto builtin = RegularBuiltinOperator{
					.symbol = ctx.query<defgen::QueryGeneratedSymbol>({
						.name = name,
						.generated_symbol_data
						= defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::BuiltinOperator{
							ctx.query<tsh::QueryFunctionType>({
								.parameter_types = std::move(param_types),
								.result_type     = return_type,
							}),
							operatoriness,
						} },
					}),
					.op     = [&]() -> RegularBuiltinOperator::HOUTRepresentation {
						if (v_matches(bop, BuiltinUnary)) return std::get<BuiltinUnary>(bop);
						if (v_matches(bop, BuiltinBinary)) return std::get<BuiltinBinary>(bop);
						CORE_UNREACHABLE();
					}()
				};
				result_ops.put(builtin.symbol, builtin);
			};
			// - Helper function to register a builtin operation that results in a function call.
			const auto builtin_call = [&ctx, &result_ops](
										  const base::StrID                            name,
										  const std::vector<tsh::SymbolType<>>&        param_types,
										  const tsh::SymbolType<>                      return_type,
										  const base::StrID                            builtin_name,
										  const HOUTFunctionDeclaration::Operatoriness operatoriness
									  ) -> void {
				const auto gen_data
					= defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::BuiltinOperator{
						ctx.query<tsh::QueryFunctionType>({
							.parameter_types = param_types,
							.result_type     = return_type,
						}),
						operatoriness,
					} };
				auto builtin = RegularBuiltinOperator{
					.symbol = ctx.query<defgen::QueryGeneratedSymbol>({
						.name                  = name,
						.generated_symbol_data = gen_data,
					}),
					.op
					= RegularBuiltinOperator::FunctionCall{ ctx.query<defgen::QueryGeneratedSymbol>(
						{ .name = builtin_name, .generated_symbol_data = gen_data }
					) },
				};
				result_ops.put(builtin.symbol, builtin);
			};

			/// Boolean operations ///
			builtin_op(keywordToStr(lang_def::Keyword::Not), { bool_t }, bool_t, BooleanNot, Prefix);

			/// Meta constructions ///
			const auto meta_t = tsh::SymbolType<>{
				tsh::getMetaType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};

			builtin_op(keywordToStr(Keyword::Ref), { meta_t }, meta_t, Ref, Prefix);
			builtin_op(keywordToStr(Keyword::Box), { meta_t }, meta_t, Box, Prefix);
			builtin_op(keywordToStr(Keyword::Ptr), { meta_t }, meta_t, Ptr, Prefix);
			builtin_op(keywordToStr(Keyword::ManyPtr), { meta_t }, meta_t, ManyPtr, Prefix);
			builtin_op(keywordToStr(Keyword::CPtr), { meta_t }, meta_t, CPtr, Prefix);
			builtin_op(keywordToStr(Keyword::Slice), { meta_t }, meta_t, Slice, Prefix);
			builtin_op(keywordToStr(Keyword::Const), { meta_t }, meta_t, Const, Prefix);

			/// Meta comparisons ///
			builtin_op(base::StrID("=="), { meta_t, meta_t }, bool_t, MetaEq, Infix);
			builtin_op(base::StrID("!="), { meta_t, meta_t }, bool_t, MetaNeq, Infix);

			/// Boolean operations ///
			const auto kw_and = keywordToStr(Keyword::And);
			const auto kw_or  = keywordToStr(Keyword::Or);
			builtin_op(kw_and, { bool_t, bool_t }, bool_t, BooleanAnd, Infix);
			builtin_op(kw_or, { bool_t, bool_t }, bool_t, BooleanOr, Infix);
			builtin_op(base::StrID("=="), { bool_t, bool_t }, bool_t, IntegerEq, Infix);
			builtin_op(base::StrID("!="), { bool_t, bool_t }, bool_t, IntegerNeq, Infix);

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
			builtin_op(base::StrID("<"), { char_t, char_t }, bool_t, IntegerLt, Infix);
			builtin_op(base::StrID("<="), { char_t, char_t }, bool_t, IntegerLteq, Infix);
			builtin_op(base::StrID(">"), { char_t, char_t }, bool_t, IntegerGt, Infix);
			builtin_op(base::StrID(">="), { char_t, char_t }, bool_t, IntegerGteq, Infix);
			builtin_op(base::StrID("=="), { char_t, char_t }, bool_t, IntegerEq, Infix);
			builtin_op(base::StrID("!="), { char_t, char_t }, bool_t, IntegerNeq, Infix);
			builtin_op(base::StrID("-"), { char_t, char_t }, u8_t, IntegerSub, Infix);
			builtin_op(base::StrID("+"), { u8_t, char_t }, char_t, IntegerAdd, Infix);
			builtin_op(base::StrID("+"), { char_t, u8_t }, char_t, IntegerAdd, Infix);

			/// String operators ///
			const auto str_t = tsh::SymbolType<>{
				tsh::getStringType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable,
			};
			builtin_call(
				base::StrID("+:"),
				{ char_t, str_t },
				str_t,
				base::StrID("builtin_string_prepended"),
				Infix
			);
			builtin_call(
				base::StrID(":+"),
				{ str_t, char_t },
				str_t,
				base::StrID("builtin_string_appended"),
				Infix
			);
			builtin_call(
				base::StrID("++"),
				{ str_t, str_t },
				str_t,
				base::StrID("builtin_string_concatenated"),
				Infix
			);

			// Return
			return result_ops;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRegularBuiltinOperatorSymbols)
}
