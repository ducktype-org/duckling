#include "query_hout_of_expr.hpp"

#include "coercions.hpp"
#include "errors.hpp"
#include "function_calls/call_processing.hpp"
#include "hout_of_subexpr.hpp"
#include "numeric_literals.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/format_string_sub_elements/format_sub_expression.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/format_string_sub_elements/format_sub_string.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/hout_creation/expressions/builtin_operators.hpp>
#include <helios_private/hout_creation/expressions/chain_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <unordered_set>

namespace compiler::helios::code {

	namespace {
		/**
		 * @brief This error message is used when there is a string literal with escape sequences
		 * that failed to parse.
		 */
		class UnknownEscapeSequenceError final: public dia_int::MessageWithCodeFragmentAndCause {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "error",
					     .family        = "parser",
					     .name          = "unknown_escape_sequence" };
			}

			class SupportedEscapeSequencesDocs final: public dia_int::MessageBase {
				dia_int::Metadata getMetadata() const final {
					return { .template_type = "message",
						     .type          = "docs",
						     .family        = "expressions",
						     .name          = "supported_escape_sequences" };
				}

			public:
				SupportedEscapeSequencesDocs(): MessageBase() {}
			};

		public:
			UnknownEscapeSequenceError(dia_int::StablePosition source_position, std::string sequence):
				  MessageWithCodeFragmentAndCause(source_position) {
				addArgument<dia_int::TextArgument>("sequence", std::move(sequence));
				addAttachedMessage(makeBox<SupportedEscapeSequencesDocs>());
			}
		};

		/**
		 * @brief This error message is used when a character literal contains more than one
		 * character.
		 */
		class InvalidCharacterLiteralError final: public dia_int::MessageWithCodeFragment {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "error",
					     .family        = "parser",
					     .name          = "invalid_character_literal" };
			}

		public:
			explicit InvalidCharacterLiteralError(dia_int::StablePosition source_position):
				  MessageWithCodeFragment(source_position) {}
		};

		void getVariantSubExprsInPlace(
			query::Context&                                   ctx,
			pst::AccessLocked<pst::ExprElement>               expr,
			std::vector<pst::AccessLocked<pst::ExprElement>>& sub_exprs_append
		) {
			if (auto bin_op_opt = expr.unlock(ctx).dynamicCast<pst::expr::BinaryOperator>()) {
				auto bin_op = bin_op_opt.value();
				if (bin_op->getOperator().unlock(ctx)->unwrap() == lang_def::NamedOperator::Pipe) {
					getVariantSubExprsInPlace(ctx, bin_op->getLeftOperand(), sub_exprs_append);
					getVariantSubExprsInPlace(ctx, bin_op->getRightOperand(), sub_exprs_append);
				}
			} else {
				sub_exprs_append.emplace_back(expr);
			}
		}

		/**
		 * @brief Extracts sub expressions from a variant operator.
		 * This flattens PST `a | b | c` expression (only if there are no parenthesis).
		 */
		std::vector<pst::AccessLocked<pst::ExprElement>> getVariantSubExprs(
			query::Context& ctx, pst::AccessLocked<pst::expr::BinaryOperator> expr
		) {
			CORE_ASSERT(
				expr.unlock(ctx)->getOperator().unlock(ctx)->unwrap()
					== lang_def::NamedOperator::Pipe,
				"Not a variant operator"
			);
			std::vector<pst::AccessLocked<pst::ExprElement>> sub_exprs;
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getLeftOperand(), sub_exprs);
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getRightOperand(), sub_exprs);
			return sub_exprs;
		}

		std::vector<SymID> filterFunctionsByOperatoriness(
			query::Context&                              ctx,
			const std::vector<SymID>&                    function_syms,
			const HOUTFunctionDeclaration::Operatoriness operatoriness
		) {
			std::vector<SymID> result;
			for (const auto& sym: function_syms)
				if (const auto sym_decl = ctx.query<QueryDeclOfFun>(sym);
				    sym_decl->valueOrThrow().operatoriness == operatoriness)
					result.push_back(sym);
			return result;
		}

		/**
		 * Visitor that implements logic of creation of HOUT expressions from PST expressions.
		 */
		struct PstExprToHoutExprVisitor final: public pst::expr::PstExprVisitorPanicky {
			explicit PstExprToHoutExprVisitor(query::Context& ctx): ctx(ctx) {}

			query::Context& ctx;

			/**
			 * The "output" of the visitor.
			 */
			base::Optional<Box<Expr>> node;

			void visitUnitExpr(pst::Access<pst::expr::UnitExpr> elem) override {
				node = makeBox<LiteralUnitExpr>(ctx, pstOrigin(elem));
			}

			void visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> stmt) override {
				auto parsed_numeric_value = fromExprNumericValue(ctx, stmt);

				if (parsed_numeric_value.has_value()) {
					node = makeBox<LiteralNumericExpr>(
						ctx, pstOrigin(stmt), parsed_numeric_value.value()
					);
				} else {
					// Error was logged in fromExprNumericValue.
					return;
				}
			}

			void visitExprCharValue(pst::Access<pst::expr::ExprCharValue> stmt) override {
				const auto escaped_string  = stmt->getValue().value.strView();
				const auto unescape_result = base::unescapeString(escaped_string);
				match_optional(unescape_result) {
					opt_some(result) {
						if (result.value.size() == 1) {
							node
								= makeBox<LiteralCharExpr>(ctx, pstOrigin(stmt), result.value.at(0));
						} else {
							ctx.logInt(
								makeBox<InvalidCharacterLiteralError>(stmt->getStablePosition())
							);
						}
					}
					opt_err(error) {
						ctx.logInt(makeBox<UnknownEscapeSequenceError>(
							stmt->getStablePosition(), error.value
						));
					}
				}
			}

			void visitExprStrValue(pst::Access<pst::expr::ExprStrValue> stmt) override {
				const auto escaped_string  = stmt->getValue().value.strView();
				const auto unescape_result = base::unescapeString(escaped_string);
				match_optional(unescape_result) {
					opt_some(result) {
						node = makeBox<LiteralStringExpr>(
							ctx, pstOrigin(stmt), base::StrID(result.value)
						);
					}
					opt_err(error) {
						ctx.logInt(makeBox<UnknownEscapeSequenceError>(
							stmt->getStablePosition(), error.value
						));
					}
				}
			}

			void visitExprFormatStrValue(pst::Access<pst::expr::ExprFormatStrValue> stmt) override {
				auto concat_sym = defgen::concatSym(ctx);

				// Construct the expression, initially empty.
				MBox<Expr> result_expr
					= makeBox<LiteralStringExpr>(ctx, generatedOrigin(), base::StrID(""));
				bool failed = false;

				// - For each sub element
				for (auto sub_locked: stmt->getSubElements()) {
					const auto sub         = sub_locked.unlock(ctx);
					MBox<Expr> next_string = nullptr;

					// - Create the next string expression
					if (const auto substr = sub.dynamicCast<pst::FormatSubString>()) {
						// - If it's a string, take it as a literal, remember to unescape it
						const auto escaped_string  = substr.value()->getValue().value.strView();
						const auto unescape_result = base::unescapeString(escaped_string);
						match_optional(unescape_result) {
							opt_some(result) {
								next_string = defgen::getStringFromLiteralExpr(
									ctx, base::StrID(result.value)
								);
							}
							opt_err(error) {
								ctx.logInt(makeBox<UnknownEscapeSequenceError>(
									stmt->getStablePosition(), error.value
								));
							}
						}
					} else if (const auto sub_expr = sub.dynamicCast<pst::FormatSubExpression>()) {
						// - If it's an expression, use its toString() method
						// - - Parse sub expression and gather data
						auto sub_expr_hout_qresult = subExprFromPST(
							ctx, sub_expr.value()->getExpr().unlock(ctx)->getExpr()
						);
						if (sub_expr_hout_qresult.hasFailed()) {
							failed = true;
							continue;
						}
						auto       sub_expr_hout = std::move(sub_expr_hout_qresult).valueOrThrow();
						const auto sub_expr_type = sub_expr_hout->expression_type.getType();
						const auto to_string_sym = defgen::toStringSymForType(ctx, sub_expr_type);

						// - - Correct for passing by copy or reference depending on type
						if (not sub_expr_hout->expression_type.getType().isSimple()
						    and sub_expr_hout->expression_type.getSymbolType().getRefKind()
						            != tsh::ReferenceKind::Ref) {
							sub_expr_hout = makeBox<RefOfExpr>(
								ctx, generatedOrigin(), std::move(sub_expr_hout)
							);
						}
						if (sub_expr_hout->expression_type.getType().isSimple()
						    and sub_expr_hout->expression_type.getSymbolType().getRefKind()
						            != tsh::ReferenceKind::Direct) {
							sub_expr_hout = makeBox<DerefExpr>(
								ctx, generatedOrigin(), std::move(sub_expr_hout)
							);
						}

						// - - Create HOUT Expr
						std::vector<Box<Expr>> arguments;
						arguments.emplace_back(std::move(sub_expr_hout));
						next_string = makeBox<CallExpr>(
							ctx,
							pstOrigin(sub).generatedFrom(),
							makeBox<IdentifierExpr>(
								ctx, pstOrigin(sub).generatedFrom(), to_string_sym
							),
							std::move(arguments)
						);
					} else {
						CORE_UNREACHABLE();
					}

					if (not next_string) {
						failed = true;
						continue;
					}

					// - Concatenate the result with the next string.
					std::vector<Box<Expr>> arguments;
					arguments.emplace_back(std::move(result_expr).toOptBox().value());
					arguments.emplace_back(std::move(next_string).toOptBox().value());

					result_expr = makeBox<CallExpr>(
						ctx,
						pstOrigin(sub).generatedFrom(),
						makeBox<IdentifierExpr>(ctx, pstOrigin(sub).generatedFrom(), concat_sym),
						std::move(arguments)
					);
				}

				// If any sub-expression failed to be processed, fail the entire visit.
				if (failed) return;

				node = std::move(result_expr).toOptBox().value();
			}

			static bool isNumericType(const tsh::AbstractType type) {
				return type.getKind() == tsh::Kind::Integral or type.getKind() == tsh::Kind::Float;
			}

			static bool isNumericOperator(const lexer::Operator op) {
				// Only operators which allow their arguments to undergo numeric promotion.
				static const std::set<std::string> numeric_ops
					= { "+", "-", "*", "/", "%", "**", "<", "<=", ">", ">=", "==", "!=" };
				return numeric_ops.contains(op.str());
			}

			/**
			 * @brief Finds the appropriate unary operator to call and constructs the corresponding
			 * HOUT expression. Consumes the provided argument expression.
			 * Some cases, such as the ampersand and asterisk for references are not handled here.
			 * Perhaps they will be moved here later.
			 * @param op The operator
			 * @param inner The precomputed argument
			 * @param scope The scope in which the operator call happens
			 * @param operatoriness Whether the operator is prefix or suffix
			 */
			[[nodiscard]]
			Box<Expr> resolveUnaryOperator(
				const pst::Access<pst::OperatorWrapper>      op,
				Box<Expr>                                    inner,
				const ScopeID                                scope,
				const HOUTFunctionDeclaration::Operatoriness operatoriness
			) const {
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
				if (isNumericType(inner->expression_type.getType())
				    && isNumericOperator(op->unwrap())) {
					auto numeric_builtin_opt
						= findNumericUnaryBuiltin(ctx, op->unwrap(), inner.ref());
					auto new_origin
						= operatoriness == HOUTFunctionDeclaration::Operatoriness::Prefix
					        ? elementOriginOrdered(pstOrigin(op), inner->origin)
					        : elementOriginOrdered(inner->origin, pstOrigin(op));

					if_opt_some(numeric_builtin_opt, numeric_builtin) {
						auto [operation, coercion] = numeric_builtin;
						auto coerced_inner         = coercion.coerce(ctx, std::move(inner));
						return makeBox<UnaryOperatorExpr>(
							ctx, new_origin, operation, std::move(coerced_inner)
						);
					}
				}

				// Step 2. — Regular lookup and overload resolution
				const auto lookup_result
					= HInterface::ofScopeWithParents(scope).lookup(ctx, op->unwrap().value);
				// @TODO: #1412 fix dealias
				auto all_candidates = lookup_result->valueOrThrow().leaves;
				for (const auto [builtin_operator_sym, _]:
				     *ctx.query<QueryRegularBuiltinOperatorSymbols>({})) {
					if (name(builtin_operator_sym) == op->unwrap().value)
						all_candidates.push_back(builtin_operator_sym);
				}
				all_candidates = filterFunctionsByOperatoriness(ctx, all_candidates, operatoriness);
				return processUnaryOperatorCall(
						   ctx, all_candidates, std::move(inner), pstOrigin(op), operatoriness
				)
				    .valueOrThrow();
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator> stmt) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Suffix operators are not implemented yet in HOUT, since they don't exist yet.",
					stmt->getStablePosition()
				));
				return;  // failed
			}

			void visitPrefixOperator(pst::Access<pst::expr::PrefixOperator> stmt) override {
				const auto op         = stmt->getOperator().unlock(ctx);
				auto       inner      = subExprFromPST(ctx, stmt->getExpr()).valueOrThrow();
				auto       inner_type = inner->expression_type.getSymbolType();

				// Handle taking references
				if (op->unwrap() == lang_def::NamedOperator::Ampersand) {
					// @TODO: #1956 remove the check bellow.
					// This is a temporary check to prevent us from taking reference of types that
					// do not carry information.
					if (not inner_type.getType().carriesInformation(ctx)) {
						ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
							"Taking reference of type that does not carry information is not "
							"supported yet.",
							stmt->getStablePosition()
						));
						return;  // failed
					}
					// @TODO: #1549 RefOfExpr is inserted here naively without any checks.
					// This should change to take value category into consideration as well as the
					// `unique`/`leaking` specifiers.
					auto primary_category = inner->expression_type.getValueCategory().getCategory();
					if (primary_category == tsh::PrimaryCategory::Literal
					    || primary_category == tsh::PrimaryCategory::Temporary) {
						ctx.logInt(makeBox<dia_int::PlaceholderError>(
							"Tried to reference a temporary", stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<RefOfExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				// Handle dereferencing
				if (op->unwrap() == lang_def::NamedOperator::Multiply) {
					if (not tsh::isPointerKind(inner_type.getType().getKind())) {
						ctx.logInt(makeBox<dia_int::PlaceholderError>(
							"Tried to dereference a non-pointer type", stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<DerefExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::Move)) {
					node = makeBox<MoveExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				node = resolveUnaryOperator(
					op,
					std::move(inner),
					ctx.query<QueryPrimaryCodeScopeFor>({ stmt }),
					HOUTFunctionDeclaration::Operatoriness::Prefix
				);
			}

			/**
			 * @brief Finds the appropriate binary operator to call and constructs the corresponding
			 * HOUT expression. Consumes the provided expressions of the arguments.
			 * Currently used for all operators other than `As` (type cast) and `Pipe` (variant type
			 * construction). Perhaps they will be moved here later.
			 * @param op The operator
			 * @param lhs The precomputed left-hand side argument
			 * @param rhs The precomputed right-hand side argument
			 * @param scope The scope in which the operator call happens
			 */
			[[nodiscard]]
			Box<Expr> resolveBinaryOperator(
				pst::Access<pst::OperatorWrapper> op, Box<Expr> lhs, Box<Expr> rhs, ScopeID scope
			) const {
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
				if (isNumericType(lhs_type.getType()) && isNumericType(rhs_type.getType())
				    && isNumericOperator(op->unwrap())) {
					auto numeric_builtin_opt
						= findNumericBinaryBuiltin(ctx, op->unwrap(), lhs.ref(), rhs.ref());
					auto new_origin = elementOriginOrdered(lhs->origin, rhs->origin);

					if_opt_some(numeric_builtin_opt, numeric_builtin) {
						auto [operation, lhs_coercion, rhs_coercion] = numeric_builtin;
						auto coerced_lhs = lhs_coercion.coerce(ctx, std::move(lhs));
						auto coerced_rhs = rhs_coercion.coerce(ctx, std::move(rhs));
						return makeBox<BinaryOperatorExpr>(
							ctx, new_origin, operation, std::move(coerced_lhs), std::move(coerced_rhs)
						);
					}
				}

				// Step 2. — Regular lookup and overload resolution
				const auto lookup_result
					= HInterface::ofScopeWithParents(scope).lookup(ctx, op->unwrap().value);
				// @TODO: #1412 fix dealias
				auto all_candidates = lookup_result->valueOrThrow().leaves;
				for (const auto [builtin_operator_sym, _]:
				     *ctx.query<QueryRegularBuiltinOperatorSymbols>({})) {
					if (name(builtin_operator_sym) == op->unwrap().value)
						all_candidates.push_back(builtin_operator_sym);
				}
				all_candidates = filterFunctionsByOperatoriness(
					ctx, all_candidates, HOUTFunctionDeclaration::Operatoriness::Infix
				);
				return processBinaryOperatorCall(
						   ctx, all_candidates, std::move(lhs), std::move(rhs), pstOrigin(op)
				)
				    .valueOrThrow();
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// Handle explicit type conversions
				const auto op = stmt->getOperator().unlock(ctx);
				if (op->unwrap() == lang_def::NamedOperator::As) {
					const auto meta_type = tsh::SymbolType<>::withDefaults(tsh::getMetaType());

					auto value_expr = subExprFromPST(ctx, stmt->getLeftOperand()).valueOrThrow();
					auto type_expr = subExprFromPSTWithType(ctx, stmt->getRightOperand(), meta_type)
					                     .valueOrThrow();
					auto type_ctv    = ctx.query<QueryEvaluateHOUTExpression>({ type_expr.ref() });
					auto symbol_type = type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value();

					assertCastIsValid(
						stmt, value_expr->expression_type.getSymbolType(), symbol_type
					);
					node = makeBox<CastExpr>(
						ctx, pstOrigin(stmt), std::move(value_expr), symbol_type
					);
					return;
				}

				// Handle variant type construction
				if (op->unwrap() == lang_def::NamedOperator::Pipe) {
					auto                   sub_exprs = getVariantSubExprs(ctx, stmt);
					std::vector<Box<Expr>> all_subtypes;

					// Expect all subexpressions in variant constructor to be Meta types or try to
					// lift them if they aren't.
					const auto meta_type = tsh::SymbolType<>{
						tsh::getMetaType(),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					};

					for (auto sub_expr: sub_exprs) {
						auto sub_expr_hout = subExprFromPSTWithType(ctx, sub_expr, meta_type);
						if (sub_expr_hout.hasFailed()) {
							// Error has occurred.
							return;
						}
						all_subtypes.emplace_back(std::move(sub_expr_hout).valueOrThrow());
					}
					node = makeBox<VariantTypeConstructorExpr>(
						ctx, pstOrigin(stmt), std::move(all_subtypes)
					);
					return;
				}

				// Default case (typical operators, built-in or user-defined)
				auto lhs_res = subExprFromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = subExprFromPST(ctx, stmt->getRightOperand());

				auto lhs = std::move(lhs_res).valueOrThrow();
				auto rhs = std::move(rhs_res).valueOrThrow();

				node = resolveBinaryOperator(
					op, std::move(lhs), std::move(rhs), ctx.query<QueryPrimaryCodeScopeFor>({ stmt })
				);
			}

			void assertCastIsValid(
				pst::Access<pst::expr::BinaryOperator> stmt,
				const tsh::SymbolType<>&               from,
				const tsh::SymbolType<>&               to
			) {
				if (from == to) return;  // trivial cast, always valid

				using tsh::Kind;
				using tsh::Mutability;
				using tsh::ReferenceKind;

				// For now we allow casts between numeric types
				if (isNumericType(from.getType()) && isNumericType(to.getType())
				    && from.getRefKind() == ReferenceKind::Direct
				    && to.getRefKind() == ReferenceKind::Direct)
					return;

				struct CastPattern {
					/// If specified, the cast must have this reference kind on the source side. If
					/// not specified, any reference kind matches.
					base::Optional<tsh::ReferenceKind> from_ref_kind;
					/// If specified, the cast must have this kind on the source side. If not
					/// specified, any kind matches.
					base::Optional<tsh::Kind> from_kind;
					/// If specified, the cast must have this reference kind on the target side. If
					/// not specified, any reference kind matches.
					base::Optional<tsh::ReferenceKind> to_ref_kind;
					/// If specified, the cast must have this kind on the target side. If not
					/// specified, any kind matches.
					base::Optional<tsh::Kind> to_kind;
					/// If true, the cast is only valid if the symbol pointee types are the same.
					bool same_pointee_type;
				};

				static const std::vector<CastPattern> valid_pointer_casts = {
					{
						.from_ref_kind     = ReferenceKind::Ref,
						.from_kind         = {},
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = tsh::Kind::Pointer,
						.same_pointee_type = true,
					},
					{
						.from_ref_kind     = ReferenceKind::Box,
						.from_kind         = {},
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = tsh::Kind::Pointer,
						.same_pointee_type = true,
					},
					{
						.from_ref_kind     = ReferenceKind::Direct,
						.from_kind         = Kind::Pointer,
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = Kind::CPointer,
						.same_pointee_type = false,
					},
					{
						.from_ref_kind     = ReferenceKind::Direct,
						.from_kind         = Kind::ManyPointer,
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = Kind::CPointer,
						.same_pointee_type = false,
					},
					// From CPointer to Pointer is temporary
					{
						.from_ref_kind     = ReferenceKind::Direct,
						.from_kind         = Kind::CPointer,
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = Kind::Pointer,
						.same_pointee_type = false,
					},
					// From CPointer to ManypPointer is temporary
					{
						.from_ref_kind     = ReferenceKind::Direct,
						.from_kind         = Kind::CPointer,
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = Kind::ManyPointer,
						.same_pointee_type = false,
					},
					// From CPointer to CPointer
					{
						.from_ref_kind     = ReferenceKind::Direct,
						.from_kind         = Kind::CPointer,
						.to_ref_kind       = ReferenceKind::Direct,
						.to_kind           = Kind::CPointer,
						.same_pointee_type = false,
					},
				};

				bool found_match = false;
				for (auto& pattern: valid_pointer_casts) {
					auto equals = [](auto value) {
						return [v = std::move(value)](auto other) { return other == v; };
					};
					bool from_ref_match
						= pattern.from_ref_kind.map(equals(from.getRefKind())).copyValueOr(true);
					bool from_kind_match
						= pattern.from_kind.map(equals(from.getType().getKind())).copyValueOr(true);
					bool to_ref_match
						= pattern.to_ref_kind.map(equals(to.getRefKind())).copyValueOr(true);
					bool to_kind_match
						= pattern.to_kind.map(equals(to.getType().getKind())).copyValueOr(true);
					if (!from_ref_match || !from_kind_match || !to_ref_match || !to_kind_match)
						continue;
					// To check the pointee we have to be sure that we have ref or pointer.
					bool pointee_match
						= pattern.same_pointee_type
					        ? from.getPointeeSymbolType() == to.getPointeeSymbolType()
					        : true;
					if (from_ref_match && from_kind_match && to_ref_match && to_kind_match
					    && pointee_match) {
						found_match = true;
						break;
					}
				}
				// Temporarily allow casts from CPointer to Pointer and ManyPointer, with a warning.
				if (found_match && from.getType().getKind() == tsh::Kind::CPointer) {
					ctx.logInt(makeBox<dia_int::PlaceholderWarning>(
						"Casts from CPointer will be disabled in the future and only work on "
						"native targets.",
						stmt->getStablePosition(),
						"There will be a different syntax for such casts in the future."
					));
					// I do not like this syntax "as" to work on some targets and not work on
					// others. I would like to have some syntax, so that the user has to write
					// `native_cptr_cast<ptr T>(original_ctype)` or `@native v as ptr T`
					// to make it explicit that this cast is only supported on native target.
					return;
				}
				if (found_match) return;

				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"Invalid cast from type ", from.toString(), " to type ", to.toString()
					),
					stmt->getStablePosition()
				));
				query::throwFailed();
			}

			void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain_expr) override {
				node = fromChainExpr(ctx, chain_expr).valueOrThrow();
			}

			void visitRoundExpr(pst::Access<pst::expr::RoundExpr> stmt) override {
				PstExprToHoutExprVisitor vis(ctx);
				stmt->getInner().unlock(ctx)->acceptExprVisitor(vis);
				if (vis.node)
					node = makeBox<ParenthesisExpr>(ctx, pstOrigin(stmt), std::move(*vis.node));
			}

			void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> stmt) override {
				node = fromIdentifierLiteral(ctx, stmt).valueOrThrow();
			}

			void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> stmt) override {
				using enum tsh::IntegralAbstractType::Signedness;
				switch (stmt->getKeyword().unlock(ctx)->unwrap()) {
				// true, false:
				case pst::Keyword::True:
					node = makeBox<LiteralBoolExpr>(ctx, pstOrigin(stmt), true);
					break;
				case pst::Keyword::False:
					node = makeBox<LiteralBoolExpr>(ctx, pstOrigin(stmt), false);
					break;


				// types:
				case pst::Keyword::Bool:
					node = makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getBoolType());
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getCharType());
					break;

				case pst::Keyword::Str:
					node
						= makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getCharSliceType(ctx));
					break;

				case pst::Keyword::BigStr:
					node = makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getStringType());
					break;

				case pst::Keyword::Type:
					node = makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getMetaType());
					break;

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 128, Signed)
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 64, Signed)
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 32, Signed)
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 16, Signed)
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 8, Signed)
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 128, Unsigned)
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 64, Unsigned)
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 32, Unsigned)
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 16, Unsigned)
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getIntegralType(ctx, 8, Unsigned)
					);
					break;

				case pst::Keyword::f80:
					node
						= makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getFloatType(ctx, 80));
					break;
				case pst::Keyword::f128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), tsh::getFloatType(ctx, 128)
					);
					break;
				case pst::Keyword::f64:
					node
						= makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getFloatType(ctx, 64));
					break;
				case pst::Keyword::f32:
					node
						= makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getFloatType(ctx, 32));
					break;
				case pst::Keyword::f16:
					node
						= makeBox<LiteralTypeExpr>(ctx, pstOrigin(stmt), tsh::getFloatType(ctx, 16));
					break;
				case pst::Keyword::List: {
					node = makeBox<LiteralTypeExpr>(
						ctx,
						pstOrigin(stmt),
						ctx.query<tsh::QueryTypeTemplateType>(
							{ tsh::TypeTemplateAbstractType::BuiltinKind::List }
						)
					);
					break;
				}
				case pst::Keyword::Self: {
					auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

					const auto& sym_list = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
						stmt->getStablePosition(), ctx, base::StrID("self")
					);

					node = makeBox<IdentifierExpr>(
						ctx, pstOrigin(stmt), sym_list.valueOrThrow().back()
					);
					break;
				}
				default:
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Keyword not yet handled.", stmt->getStablePosition()
					));
					return;  // failed
				}
			}

			void visitComma(pst::Access<pst::expr::Comma> stmt) override {
				std::vector<Box<Expr>> expressions;
				for (auto ex: stmt->getExpressions()) {
					auto res = subExprFromPST(ctx, ex);
					if (res.hasFailed()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).valueOrThrow());
				}
				node = makeBox<TupleExpr>(ctx, pstOrigin(stmt), std::move(expressions));
			}

			void visitTernary(pst::Access<pst::expr::Ternary> stmt) override {
				const auto bool_type = tsh::SymbolType<>::withDefaults(tsh::getBoolType());
				auto condition_res   = subExprFromPSTWithType(ctx, stmt->getCondition(), bool_type);
				auto if_true_res     = subExprFromPST(ctx, stmt->getIfTrue());
				auto if_false_res    = subExprFromPST(ctx, stmt->getIfFalse());

				if (condition_res.hasFailed() or if_true_res.hasFailed() or if_false_res.hasFailed())
					return;

				auto condition = std::move(condition_res).valueOrThrow();
				auto if_true   = std::move(if_true_res).valueOrThrow();
				auto if_false  = std::move(if_false_res).valueOrThrow();

				node = makeBox<TernaryOperatorExpr>(
					ctx,
					pstOrigin(stmt),
					std::move(condition),
					std::move(if_true),
					std::move(if_false)
				);
			}

			void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> stmt) override {
				using namespace ::std::views;

				auto  expr_count     = usize(stmt->numberOfSubExpressions());
				usize operator_count = expr_count - 1;

				std::vector<Box<Expr>> result_exprs;
				result_exprs.reserve(expr_count);
				for (usize i = 0; i < expr_count; ++i) {
					auto result = subExprFromPST(ctx, stmt->getSubExpr(i));
					if (result.hasFailed()) return;
					result_exprs.push_back(std::move(result.valueOrThrow()));
				}

				// Comparison chain construction is unusual, because it reuses some of its
				// arguments. Thus, we need to appropriately construct ReusableExpr instances,
				// which we pass to operator resolution. The resolved expressions (binary operators
				// or calls) are then passed to the final ChainComparisonExpr.

				// The lhs argument of the comparison to be processed.
				auto lhs = std::move(result_exprs.at(0));
				// The scope of the entire expression.
				auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });
				// The resolved comparisons.
				std::vector<Box<Expr>> comparisons;
				comparisons.reserve(operator_count);

				// Perform operator resolution for each operator in the chain. Reuse the expressions
				// which are between two operators. The last expressions is not reused, but that's fine.
				for (usize op_idx = 0; op_idx < operator_count; op_idx++) {
					// clang-format off
					const auto op = stmt->getOperator(op_idx).unlock(ctx);
					// clang-format on

					auto rhs = makeBox<ReusableExpr>(ctx, std::move(result_exprs.at(op_idx + 1)));
					auto next_lhs = rhs->nextUse();

					comparisons.emplace_back(
						resolveBinaryOperator(op, std::move(lhs), std::move(rhs), scope)
					);
					lhs = std::move(next_lhs);
				}

				node = makeBox<ChainComparisonExpr>(ctx, pstOrigin(stmt), std::move(comparisons));
			}

			void visitAssignment(pst::Access<pst::expr::Assignment> stmt) override {
				auto op = stmt->getAssignmentType().unlock(ctx)->unwrap();

				auto var = stmt->getVariables();

				auto location_expr_qresult = subExprFromPST(ctx, var);
				if (location_expr_qresult.hasFailed()) return;
				auto location_expr = std::move(location_expr_qresult).valueOrThrow();

				// If left side of the assignment is a ref/box, dereference it first.
				auto location_type = location_expr->expression_type.getSymbolType();
				if (location_type.getRefKind() != tsh::ReferenceKind::Direct)
					location_expr = makeBox<DerefExpr>(
						ctx, location_expr->origin.generatedFrom(), std::move(location_expr)
					);

				auto location_mutability = location_type.getMutability();
				if (location_mutability == tsh::Mutability::Immutable) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Left side of assignment can't be immutable.", stmt->getStablePosition()
					));
					return;
				}

				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"'", op.str(), "' assignment for type: '", location_type.toString(), "'."
					),
					stmt->getStablePosition()
				));
			}
		};
	}

	ExprConstructionResult subExprFromPST(
		query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
	) {
		auto element_unlocked = element.unlockOpt(ctx);
		if (element_unlocked.empty()) {
			// PST should have reported parsing error for this, so we just return failure here.
			return query::Failed();
		}

		PstExprToHoutExprVisitor visitor(ctx);
		element_unlocked.value()->acceptExprVisitor(visitor);

		if_opt_some(visitor.node, expr) return std::move(expr);
		return query::Failed();
	}
}

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO static assert this is top-expr
			return code::subExprFromPST(ctx, key.element);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	query::QResult<BoxOrCRef<code::Expr>> getHoutOfExprWithExpectedType(
		query::Context&                                      ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>&     pst_expr,
		const tsh::SymbolType<>                              expected_type,
		base::Optional<std::function<void(query::Context&)>> log_error
	) {
		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });

		UNPACK_QRESULT_CREF_TO_BOX(CRef<code::Expr> expr_hout =, expr_hout_qresult);

		const auto source_symbol_type = expr_hout->expression_type.getSymbolType();
		const auto source_position    = pst_expr.element.unlock(ctx)->getStablePosition();
		const auto coercion_qresult   = canCoerce(ctx, source_symbol_type, expected_type);
		if (coercion_qresult.hasFailed()) return query::Failed();

		variant_match(coercion_qresult.valueOrThrow().getVariant()) {
			variant_case(Coercion, coercion) { return coercion.coerceFromRef(ctx, expr_hout); }
			variant_default {
				logCoercionFailure(
					ctx,
					coercion_qresult,
					source_symbol_type,
					expected_type,
					source_position,
					std::move(log_error)
				);
				return query::Failed();
			}
		}
		CORE_UNREACHABLE();
	}

	query::QResult<Box<code::Expr>> code::subExprFromPSTWithType(
		query::Context&                                      ctx,
		pst::AccessLocked<pst::ExprElement>                  element,
		tsh::SymbolType<>                                    expected_type,
		base::Optional<std::function<void(query::Context&)>> log_error
	) {
		auto expr_hout_qresult = code::subExprFromPST(ctx, element);
		UNPACK_QRESULT_MOVE(auto expr_hout =, expr_hout_qresult);
		const auto source_position = element.unlock(ctx)->getStablePosition();

		auto maybe_coerced = coerceFromBox(
			ctx, std::move(expr_hout), expected_type, source_position, std::move(log_error)
		);
		if (maybe_coerced.has_value()) return std::move(maybe_coerced.value());
		return query::Failed();
	}
}
