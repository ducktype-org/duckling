#include "operators.hpp"

#include "function_calls/call_processing.hpp"

#include <helios/queries/function_queries.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/maps.hpp>

#include <lang_definitions/key_spec_op.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <tuple>
#include <utility>
#include <vector>

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

		// Numeric operators only work on direct values. When provided with references or box types
		// we have to force a coercion to a direct type which will insert a DerefExpr. This is
		// needed to handle cases like: var x = referenceA + referenceB.
		auto lhs_direct = lhs_type.withReferenceKind(tsh::ReferenceKind::Direct);
		auto rhs_direct = rhs_type.withReferenceKind(tsh::ReferenceKind::Direct);

		// Try to coerce both values to the rhs direct type.
		auto lhs_to_rhs = canCoerce(ctx, lhs->expression_type, rhs_direct).valueOrThrow();
		auto rhs_to_rhs = canCoerce(ctx, rhs->expression_type, rhs_direct).valueOrThrow();

		if (lhs_to_rhs.isValid() && rhs_to_rhs.isValid())
			return std::make_tuple(rhs_direct, std::move(lhs_to_rhs), std::move(rhs_to_rhs));

		// Try to coerce both values to the lhs direct type.
		auto lhs_to_lhs = canCoerce(ctx, lhs->expression_type, lhs_direct).valueOrThrow();
		auto rhs_to_lhs = canCoerce(ctx, rhs->expression_type, lhs_direct).valueOrThrow();

		if (lhs_to_lhs.isValid() && rhs_to_lhs.isValid())
			return std::make_tuple(lhs_direct, std::move(lhs_to_lhs), std::move(rhs_to_lhs));

		// Invalid coercion.
		return {};
	}

	/**
	 * @brief Resolves a builtin binary numeric operator into HELIOS expression. If it cannot be
	 * resolved correctly (for example, because there is no language primitive that matches the @p
	 * lhs and @p rhs types) .
	 * @param ctx Query context
	 * @param op Operator to be resolved
	 * @param lhs Left hand side argument of the operator
	 * @param rhs Right hand side argument of the operator
	 * @return Corresponding desugared expression
	 */
	query::QResult<Box<code::Expr>> desugarOperatorToExpr(
		query::Context& ctx, BuiltinOperation operation, Box<code::Expr> lhs, Box<code::Expr> rhs
	) {
		using enum PreDesugarOperator;
		using namespace compiler::helios::code::shorthands;
		Shorthand s{ ctx };

		auto new_origin = elementOriginOrdered(lhs->origin, rhs->origin);

		if (std::holds_alternative<BuiltinBinary>(operation))
			return withOrigin(
				new_origin,
				s.binOp(std::move(lhs), std::get<BuiltinBinary>(operation), std::move(rhs))
			);

		CORE_ASSERT(
			std::holds_alternative<PreDesugarOperator>(operation),
			"Operation should be `PreDesugarOperator` at this point"
		);
		auto op = std::get<PreDesugarOperator>(operation);

		switch (op) {
		case IntegerPow: {
			if (!isLanguagePrimitivePresent(ctx, LanguagePrimitive::PowInt)) {
				ctx.logInt(makeBox<dia::PlaceholderError>(base::strConcat(
					"Calling an operator that requires '",
					base::enumToStr(LanguagePrimitive::PowInt),
					"' language primitive, but no such primitive was found."
				)));
				return query::Failed();
			}
			auto callee = bakeLanguagePrimitiveWithTypes(
				ctx, LanguagePrimitive::PowInt, { lhs->expression_type.getSymbolType() }
			);
			return withOrigin(new_origin, s.call(s.ident(callee), std::move(lhs), std::move(rhs)));
		}
		case FloatPow: {
			auto lang_primitive = [&] -> base::Optional<LanguagePrimitive> {
				auto type = lhs->expression_type.getType();
				if (type == tsh::getFloatType(ctx, 32))
					return LanguagePrimitive::PowF32;
				else if (type == tsh::getFloatType(ctx, 64))
					return LanguagePrimitive::PowF64;
				else
					return {};
			}();
			if_opt_none(lang_primitive) {
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat("Float exponentiation on unhandled type. Only `f32` and `f64` "
				                    "are handled for now.")
				));
				return query::Failed();
			}
			if (!isLanguagePrimitivePresent(ctx, lang_primitive.value())) {
				ctx.logInt(makeBox<dia::PlaceholderError>(base::strConcat(
					"Calling an operator that requires '",
					base::enumToStr(lang_primitive.value()),
					"' language primitive, but no such primitive was found."
				)));
				return query::Failed();
			}
			auto callee = ctx.query<helios::QueryLanguagePrimitiveSymID>({ lang_primitive.value() })
			                  ->valueOrThrow();
			return withOrigin(new_origin, s.call(s.ident(callee), std::move(lhs), std::move(rhs)));
		}
		}
		CORE_UNREACHABLE();
	}

	void filterFunctionsByOperatoriness(
		query::Context&                              ctx,
		std::vector<SymID>&                          function_syms,
		const HOUTFunctionDeclaration::Operatoriness opiness
	) {
		base::filterVectorInPlace(function_syms, [&](const SymID& sym) {
			return ctx.query<QueryDeclOfFun>(sym)->valueOrThrow().operatoriness == opiness;
		});
	}
}

namespace compiler::helios::code {
	bool isNumericOperator(const lexer::Operator op) {
		// Only operators which allow their arguments to undergo numeric promotion.
		static const std::set<std::string> numeric_ops
			= { "+",  "-",  "*",  "/", "%", "**", "<", "<=", ">",
			    ">=", "==", "!=", "&", "|", "^",  "~", "<<", ">>" };
		return numeric_ops.contains(op.str());
	}

	base::Optional<std::tuple<BuiltinUnary, Coercion>> findNumericUnaryBuiltin(
		query::Context& ctx, lexer::Operator op, const CRef<Expr> expr
	) {
		auto operation_kind = expr->expression_type.getType().getKind();
		auto direct_type
			= expr->expression_type.getSymbolType().withReferenceKind(tsh::ReferenceKind::Direct);
		auto coercion = canCoerce(ctx, expr->expression_type, direct_type).valueOrThrow();
		if (not coercion.isValid()) return {};

		const static base::Map<std::pair<lexer::Operator, tsh::Kind>, BuiltinUnary> numeric_operators
			= {
				  /// Negations ///
				  { { base::StrID("-"), tsh::Kind::Integral }, BuiltinUnary::IntegerNegation },
				  { { base::StrID("-"), tsh::Kind::Float }, BuiltinUnary::FloatNegation },

				  /// Bitwise negation ///
				  { { base::StrID("~"), tsh::Kind::Integral }, BuiltinUnary::IntegerBitNot },
			  };

		if (numeric_operators.contains({ op, operation_kind }))
			return std::make_tuple(numeric_operators.at({ op, operation_kind }), coercion);

		return {};
	}

	base::Optional<std::tuple<BuiltinOperation, Coercion, Coercion>> findNumericBinaryBuiltin(
		query::Context& ctx, lexer::Operator op, CRef<Expr> lhs, CRef<Expr> rhs
	) {
		auto common_type_res = findCommonTypeWithCoercion(ctx, lhs, rhs);
		if (!common_type_res.has_value()) return {};

		auto& [common_type, lhs_coercion, rhs_coercion] = common_type_res.value();

		auto operation_kind = common_type.getType().getKind();

		const static base::Map<std::pair<lexer::Operator, tsh::Kind>, BuiltinOperation>
			numeric_operators = {
				/// Integer arithmetic ///
				{ { base::StrID("+"), tsh::Kind::Integral }, BuiltinBinary::IntegerAdd },
				{ { base::StrID("-"), tsh::Kind::Integral }, BuiltinBinary::IntegerSub },
				{ { base::StrID("*"), tsh::Kind::Integral }, BuiltinBinary::IntegerMul },
				{ { base::StrID("/"), tsh::Kind::Integral }, BuiltinBinary::IntegerDiv },
				{ { base::StrID("%"), tsh::Kind::Integral }, BuiltinBinary::IntegerMod },
				{ { base::StrID("**"), tsh::Kind::Integral }, PreDesugarOperator::IntegerPow },

				/// Bitwise operations ///
				{ { base::StrID("&"), tsh::Kind::Integral }, BuiltinBinary::IntegerBitAnd },
				{ { base::StrID("|"), tsh::Kind::Integral }, BuiltinBinary::IntegerBitOr },
				{ { base::StrID("^"), tsh::Kind::Integral }, BuiltinBinary::IntegerBitXor },
				{ { base::StrID("<<"), tsh::Kind::Integral }, BuiltinBinary::IntegerShl },
				{ { base::StrID(">>"), tsh::Kind::Integral }, BuiltinBinary::IntegerShr },

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
				{ { base::StrID("**"), tsh::Kind::Float }, PreDesugarOperator::FloatPow },

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

	Box<Expr> resolveBinaryOperator(
		query::Context& ctx,
		lexer::Operator op,
		ElementOrigin   op_origin,
		Box<Expr>       lhs,
		Box<Expr>       rhs,
		ScopeID         scope
	) {
		const auto lhs_type = lhs->expression_type.getSymbolType();
		const auto rhs_type = rhs->expression_type.getSymbolType();

		// Binary operator resolution now happens in two steps:
		// 1. If the arguments are both numeric (integral or float) and the operator is a
		// built-in arithmetic operator, we look for promotions from left to right and from
		// right to left, and then use the built-in operator on the promoted-to type.
		// 2. Otherwise, we perform "regular" lookup. This includes lookups in two places:
		//    a. The calling scope (a user can define a standalone function named `+`).
		//    b. The type of the left-hand side argument (for an operator method).
		// Next, we perform typical overload resolution.

		// Step 1. — special path for numeric promotions
		if (lhs_type.getType().isNumeric() && rhs_type.getType().isNumeric()
		    && isNumericOperator(op)) {
			// TODO: remove clones
			auto numeric_builtin_opt = findNumericBinaryBuiltin(ctx, op, lhs.ref(), rhs.ref());

			if_opt_some(numeric_builtin_opt, numeric_builtin) {
				using namespace compiler::helios::code::shorthands;
				Shorthand s{ ctx };

				auto [operation, lhs_coercion, rhs_coercion] = numeric_builtin;
				auto coerced_lhs = lhs_coercion.coerce(ctx, std::move(lhs));
				auto coerced_rhs = rhs_coercion.coerce(ctx, std::move(rhs));

				return desugarOperatorToExpr(
						   ctx, operation, std::move(coerced_lhs), std::move(coerced_rhs)
				)
				    .valueOrThrow();
			}
		}

		// Step 2. — Regular lookup and overload resolution
		const auto lookup_result = HInterface::ofScopeWithParents(scope).lookup(ctx, op.value);
		// @TODO: #1412 fix dealias
		auto               all_candidates = lookup_result->valueOrThrow().leaves;
		std::vector<SymID> method_candidates;
		for (const auto [builtin_operator_sym, _]:
		     *ctx.query<QueryRegularBuiltinOperatorSymbols>({})) {
			if (name(builtin_operator_sym) == op.value)
				all_candidates.push_back(builtin_operator_sym);
		}
		filterFunctionsByOperatoriness(
			ctx, all_candidates, HOUTFunctionDeclaration::Operatoriness::Infix
		);

		// Step 2b. — Look for operator methods declared on the left-hand side's type
		// (classes only)
		const auto lhs_abstract_type = lhs_type.getType();
		if (lhs_abstract_type.getKind() == tsh::Kind::Class) {
			const auto method_lookup_result
				= HInterface::ofTypeInstance(lhs_abstract_type).lookup(ctx, op.value);
			method_candidates = method_lookup_result->valueOrThrow().leaves;
			filterFunctionsByOperatoriness(
				ctx, method_candidates, HOUTFunctionDeclaration::Operatoriness::Infix
			);

			for (const auto method_candidate: method_candidates)
				if (not std::ranges::contains(all_candidates, method_candidate))
					all_candidates.push_back(method_candidate);
		}

		return processBinaryOperatorCall(
				   ctx, all_candidates, method_candidates, std::move(lhs), std::move(rhs), op_origin
		)
		    .valueOrThrow();
	}

	Box<Expr> resolveUnaryOperator(
		query::Context&                              ctx,
		lexer::Operator                              op,
		ElementOrigin                                op_origin,
		Box<Expr>                                    inner,
		const ScopeID                                scope,
		const HOUTFunctionDeclaration::Operatoriness operatoriness
	) {
		CORE_ASSERT(
			operatoriness == HOUTFunctionDeclaration::Operatoriness::Prefix
				|| operatoriness == HOUTFunctionDeclaration::Operatoriness::Suffix,
			"resolveUnaryOperator should only filter for prefix or suffix operators"
		);
		// Unary operator resolution happens in two steps:
		// 1. If the argument is numeric (integral or float) and the operator is a built-in
		//    numeric operator, we perform any needed coercion and emit a UnaryOperatorExpr.
		// 2. Otherwise, we perform "regular" lookup. This includes lookups in two places:
		//    a. The calling scope (a user can define a standalone function named `+`).
		//    b. The type of the only argument (for an operator method).
		// Next, we perform typical overload resolution.

		// Step 1. — special path for numeric promotions
		if (inner->expression_type.getType().isNumeric() && isNumericOperator(op)) {
			auto numeric_builtin_opt = findNumericUnaryBuiltin(ctx, op, inner.ref());
			auto new_origin = operatoriness == HOUTFunctionDeclaration::Operatoriness::Prefix
			                    ? elementOriginOrdered(op_origin, inner->origin)
			                    : elementOriginOrdered(inner->origin, op_origin);

			if_opt_some(numeric_builtin_opt, numeric_builtin) {
				auto [operation, coercion] = numeric_builtin;
				auto coerced_inner         = coercion.coerce(ctx, std::move(inner));
				return makeBox<UnaryOperatorExpr>(
					ctx, new_origin, operation, std::move(coerced_inner)
				);
			}
		}

		// Step 2. — Regular lookup and overload resolution
		const auto lookup_result = HInterface::ofScopeWithParents(scope).lookup(ctx, op.value);
		// @TODO: #1412 fix dealias
		auto               all_candidates = lookup_result->valueOrThrow().leaves;
		std::vector<SymID> method_candidates;
		for (const auto [builtin_operator_sym, _]:
		     *ctx.query<QueryRegularBuiltinOperatorSymbols>({})) {
			if (name(builtin_operator_sym) == op.value)
				all_candidates.push_back(builtin_operator_sym);
		}
		filterFunctionsByOperatoriness(ctx, all_candidates, operatoriness);

		// Step 2b. — Look for operator methods declared on the operand's own type (classes only)
		const auto inner_type = inner->expression_type.getType();
		if (inner_type.getKind() == tsh::Kind::Class) {
			const auto method_lookup_result
				= HInterface::ofTypeInstance(inner_type).lookup(ctx, op.value);
			method_candidates = method_lookup_result->valueOrThrow().leaves;
			filterFunctionsByOperatoriness(ctx, method_candidates, operatoriness);

			for (const auto method_candidate: method_candidates)
				if (not std::ranges::contains(all_candidates, method_candidate))
					all_candidates.push_back(method_candidate);
		}

		return processUnaryOperatorCall(
				   ctx, all_candidates, method_candidates, std::move(inner), op_origin, operatoriness
		)
		    .valueOrThrow();
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
						= defgen::BuiltinOperator{
							ctx.query<tsh::QueryFunctionType>({
								.parameter_types = std::move(param_types),
								.result_type     = return_type,
							}),
							operatoriness,
						},
					}),
					.op     = [&]() -> RegularBuiltinOperator::HOUTRepresentation {
						if (v_matches(bop, BuiltinUnary)) return std::get<BuiltinUnary>(bop);
						if (v_matches(bop, BuiltinBinary)) return std::get<BuiltinBinary>(bop);
						CORE_UNREACHABLE();
					}()
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

			// Return
			return result_ops;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRegularBuiltinOperatorSymbols)
}
