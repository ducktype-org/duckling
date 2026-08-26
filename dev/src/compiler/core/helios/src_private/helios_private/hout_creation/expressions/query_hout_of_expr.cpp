#include "query_hout_of_expr.hpp"

#include "coercions/coercions.hpp"
#include "coercions/errors.hpp"
#include "function_calls/call_processing.hpp"
#include "hout_of_subexpr.hpp"
#include "numeric_literals.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/format_string_sub_elements/format_sub_expression.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/format_string_sub_elements/format_sub_string.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/hout_creation/desugaring/match.hpp>
#include <helios_private/hout_creation/expressions/builtin_operators.hpp>
#include <helios_private/hout_creation/expressions/casts.hpp>
#include <helios_private/hout_creation/expressions/chain_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "base/extend_cpp/variant_match.hpp"
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/placeholder.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::code {

	namespace {
		using namespace shorthands;

		/**
		 * @brief This error message is used when there is a string literal with escape sequences
		 * that failed to parse.
		 */
		class UnknownEscapeSequenceError final: public dia::MessageWithCodeFragmentAndCause {
			dia::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "error",
					     .family        = "parser",
					     .name          = "unknown_escape_sequence" };
			}

			class SupportedEscapeSequencesDocs final: public dia::MessageBase {
				dia::Metadata getMetadata() const final {
					return { .template_type = "message",
						     .type          = "docs",
						     .family        = "expressions",
						     .name          = "supported_escape_sequences" };
				}

			public:
				SupportedEscapeSequencesDocs(): MessageBase() {}
			};

		public:
			UnknownEscapeSequenceError(dia::StablePosition source_position, std::string sequence):
				  MessageWithCodeFragmentAndCause(source_position) {
				addArgument<dia::TextArgument>("sequence", std::move(sequence));
				addAttachedMessage(makeBox<SupportedEscapeSequencesDocs>());
			}
		};

		/**
		 * @brief This error message is used when a character literal contains more than one
		 * character.
		 */
		class InvalidCharacterLiteralError final: public dia::MessageWithCodeFragment {
			dia::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "error",
					     .family        = "parser",
					     .name          = "invalid_character_literal" };
			}

		public:
			explicit InvalidCharacterLiteralError(dia::StablePosition source_position):
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

		void filterFunctionsByOperatoriness(
			query::Context&                              ctx,
			std::vector<SymID>&                          function_syms,
			const HOUTFunctionDeclaration::Operatoriness opiness
		) {
			base::filterVectorInPlace(function_syms, [&](const SymID& sym) {
				return ctx.query<QueryDeclOfFun>(sym)->valueOrThrow().operatoriness == opiness;
			});
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
				// Build the formatted string as `""` accumulated through a chain of by-value append
				//  `String("").append(part0).append(..)...append(partn).toString()`
				const auto append_sym = defgen::stringAppendMethodSym(ctx, false);

				const Shorthand s{ ctx };

				// Construct the expression, initially an empty String.
				Box<Expr> result_expr = s.litStrObj(base::StrID(""));
				bool      failed      = false;

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
								next_string = s.litStrObj(base::StrID(result.value));
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
						sub_expr_hout = s.prepToPassSelf(std::move(sub_expr_hout));

						// - - Create HOUT Expr. Both the call and its builtin `toString` callee
						// reference carry the substitution's (generated) origin.
						next_string = withOrigin(
							pstOrigin(sub).generatedFrom(),
							s.call(
								withOrigin(pstOrigin(sub).generatedFrom(), s.ident(to_string_sym)),
								std::move(sub_expr_hout)
							)
						);
					} else {
						CORE_UNREACHABLE();
					}

					if (failed or not next_string) {
						failed = true;
						continue;
					}

					// - Append the next string to the result.
					result_expr = withOrigin(
						pstOrigin(sub).generatedFrom(),
						s.call(
							withOrigin(pstOrigin(sub).generatedFrom(), s.ident(append_sym)),
							s.prepToPassSelf(std::move(result_expr)),
							std::move(next_string).toOptBox().value()
						)
					);
				}

				// If any sub-expression failed to be processed, fail the entire visit.
				if (failed) return;

				// The `append` chain yields a `ref String`; convert it to an owned `String` value
				// via `toString()`.
				const auto string_to_string_sym
					= defgen::toStringSymForType(ctx, tsh::getStringType(ctx));
				result_expr = withOrigin(
					pstOrigin(stmt).generatedFrom(),
					s.call(
						withOrigin(pstOrigin(stmt).generatedFrom(), s.ident(string_to_string_sym)),
						s.prepToPassSelf(std::move(result_expr))
					)
				);

				node = std::move(result_expr);
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
				filterFunctionsByOperatoriness(ctx, all_candidates, operatoriness);

				// Step 2b. — If nothing was found in the calling scope or among builtins, fall
				// back to an operator method declared on the operand's own type.
				// @TODO: #3133 This should be unified.
				if (all_candidates.empty()) {
					const auto inner_type = inner->expression_type.getType();
					const auto method_lookup_result
						= HInterface::ofTypeInstance(inner_type).lookup(ctx, op->unwrap().value);
					auto method_candidates = method_lookup_result->valueOrThrow().leaves;
					filterFunctionsByOperatoriness(ctx, method_candidates, operatoriness);
					if (!method_candidates.empty()) {
						auto self_expr = Shorthand{ ctx }.prepToPassSelf(std::move(inner));
						return processUnaryOperatorCall(
								   ctx,
								   method_candidates,
								   std::move(self_expr),
								   pstOrigin(op),
								   operatoriness
						)
						    .valueOrThrow();
					}
				}

				return processUnaryOperatorCall(
						   ctx, all_candidates, std::move(inner), pstOrigin(op), operatoriness
				)
				    .valueOrThrow();
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator> stmt) override {
				const auto op    = stmt->getOperator().unlock(ctx);
				auto       inner = subExprFromPST(ctx, stmt->getExpr()).valueOrThrow();
				// If necessary, this is the place to handle any particularly tricky cases.
				// Currently, there are none.

				// After the tricky cases have been handled, execute standard procedures.
				node = resolveUnaryOperator(
					op,
					std::move(inner),
					ctx.query<QueryPrimaryCodeScopeFor>({ stmt }),
					HOUTFunctionDeclaration::Operatoriness::Suffix
				);
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
						ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
							"Taking reference of type that does not carry information is not "
							"supported yet.",
							stmt->getStablePosition()
						));
						return;  // failed
					}
					// @TODO: #3109 Take `unique`/`leaking` specifiers into consideration.
					if (not inner->expression_type.getValueCategory().addressable()) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							"Tried to reference a temporary", stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<RefOfExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				// Handle taking pointers. Unlike `&`, `ptrof` keeps the reference kind of the
				// operand, so `ptrof` of a `box T` place is a `ptr box T` addressing the box itself.
				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::Ptrof)) {
					// @TODO: #1956 remove the check below.
					// This is a temporary check to prevent us from taking the address of types that
					// do not carry information.
					if (not inner_type.getType().carriesInformation(ctx)) {
						ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
							"Taking a pointer to a type that does not carry information is not "
							"supported yet.",
							stmt->getStablePosition()
						));
						return;  // failed
					}
					if (not inner->expression_type.getValueCategory().addressable()) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							"Tried to take a pointer to a temporary", stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<PtrOfExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				// Handle dereferencing
				if (op->unwrap() == lang_def::NamedOperator::Multiply) {
					if (not tsh::isPointerKind(inner_type.getType().getKind())) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							"Tried to dereference a non-pointer type", stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<DerefExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::Move)) {
					// `move x` is only valid on an owned lvalue (a local variable).
					if (not inner->expression_type.getValueCategory().isMovableFrom()) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							"`move` can only be applied to an owned local variable.",
							stmt->getStablePosition()
						));
						return;
					}
					node = makeBox<MoveExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				// Box creation
				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::New)) {
					const auto origin = inner->origin.generatedFrom();
					const auto direct_type
						= inner->expression_type.getSymbolType().withReferenceKind(
							tsh::ReferenceKind::Direct
						);

					auto value = coerceFromBox(
						ctx, std::move(inner), direct_type, stmt->getStablePosition(), {}
					);

					if (value.has_value())
						node = makeBoxAllocCall(ctx, origin, std::move(value.value()));
					return;
				}

				// `copy x` produces an explicit copy of `x` via its copy constructor.
				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::Copy)) {
					const Shorthand s{ ctx };

					// `copy` always creates a direct value.
					// copy: T -> T
					// copy: ref T -> T
					// copy: box T -> T
					const bool is_indirect = inner_type.getRefKind() != tsh::ReferenceKind::Direct;
					const tsh::SymbolType<> copied_type
						= is_indirect ? inner_type.getPointeeSymbolType() : inner_type;

					if (copied_type.isTriviallyCopyable(ctx)) {
						ctx.logInt(makeBox<dia::PlaceholderWarning>(
							base::strConcat(
								"Type `",
								copied_type.toString(),
								"` is trivially copyable. No need to use the explicit `copy` "
								"keyword."
							),
							stmt->getStablePosition()
						));
						// Trivially copyable, so we just deref.
						if (is_indirect)
							node = s.deref(std::move(inner));
						else
							node = std::move(inner);
						return;
					}
					if (not copied_type.getType().isCopyable(ctx)) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							base::strConcat("Type `", copied_type.toString(), "` cannot be copied."),
							stmt->getStablePosition()
						));
						return;
					}

					const auto abstract_type = copied_type.getType();
					const auto kind          = abstract_type.getKind();
					CORE_ASSERT(
						kind == tsh::Kind::Class or kind == tsh::Kind::StaticArray
							or kind == tsh::Kind::Tuple or kind == tsh::Kind::Variant,
						"Tried to call a copy constructor of a type which shouldn't need one"
					);

					// The copy constructor takes a `const ref` and returns a direct value. `refOf`
					// turns every type into a reference (including ref/box because of reference
					// kind collapsing).
					node = s.call(
						s.ident(defgen::copyConstructorSymForType(ctx, abstract_type)),
						s.refOf(std::move(inner))
					);
					return;
				}

				// `copyof x` copies `x` while keeping its full symbol type (including reference kind).
				if (op->unwrap() == lang_def::keywordToStr(lang_def::Keyword::Copyof)) {
					const Shorthand s{ ctx };

					// copyof: T -> T
					// copyof: ref T -> ref T
					// copyof: box T -> box T
					// `SymbolType::isCopyable` answers this for the value as a whole: a `ref` is
					// always copyable (the reference is copied), while a `box` follows its pointee.
					if (not inner_type.isCopyable(ctx)) {
						ctx.logInt(makeBox<dia::PlaceholderError>(
							base::strConcat("Type `", inner_type.toString(), "` cannot be copied."),
							stmt->getStablePosition()
						));
						return;
					}

					node = s.copyValue(std::move(inner), pstOrigin(stmt).generatedFrom());
					return;
				}

				// After the tricky cases have been handled, execute standard procedures.
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
				Shorthand  s{ ctx };

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
					auto numeric_builtin_opt = resolveNumericBinaryBuiltin(
						ctx, op->unwrap(), std::move(lhs), std::move(rhs)
					);

					if_opt_some(numeric_builtin_opt, numeric_builtin) {
						return std::move(numeric_builtin);
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
				filterFunctionsByOperatoriness(
					ctx, all_candidates, HOUTFunctionDeclaration::Operatoriness::Infix
				);

				// Step 2b. — if nothing was found in the calling scope or among builtins, fall
				// back to an operator method declared on the left-hand side's own type.
				// @TODO: #3133 This should be unified.
				if (all_candidates.empty()) {
					const auto lhs_abstract_type    = lhs_type.getType();
					const auto method_lookup_result = HInterface::ofTypeInstance(lhs_abstract_type)
					                                      .lookup(ctx, op->unwrap().value);
					auto method_candidates = method_lookup_result->valueOrThrow().leaves;
					filterFunctionsByOperatoriness(
						ctx, method_candidates, HOUTFunctionDeclaration::Operatoriness::Infix
					);
					if (!method_candidates.empty()) {
						auto self_expr = s.prepToPassSelf(std::move(lhs));
						return processBinaryOperatorCall(
								   ctx,
								   method_candidates,
								   std::move(self_expr),
								   std::move(rhs),
								   pstOrigin(op)
						)
						    .valueOrThrow();
					}
				}

				return processBinaryOperatorCall(
						   ctx, all_candidates, std::move(lhs), std::move(rhs), pstOrigin(op)
				)
				    .valueOrThrow();
			}

			void visitMatchExpr(pst::Access<pst::expr::MatchExpr> stmt) override {
				auto desugared = desugaring::desugarMatch(ctx, stmt);
				if (desugared.hasFailed()) return;
				node = std::move(desugared).valueOrThrow();
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// Handle explicit type conversions
				const auto op = stmt->getOperator().unlock(ctx);
				if (op->unwrap() == lang_def::NamedOperator::As) {
					auto value_expr  = subExprFromPST(ctx, stmt->getLeftOperand()).valueOrThrow();
					auto type_ctv    = getTypeCTVFromPST(ctx, stmt->getRightOperand());
					auto symbol_type = type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value();

					node = castAs(ctx, std::move(value_expr), symbol_type, stmt);
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
						auto sub_expr_hout = subExprFromPSTWithType(
							ctx, sub_expr, meta_type, op->getStablePosition()
						);
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

			void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain_expr) override {
				node = fromChainExpr(ctx, chain_expr).valueOrThrow();
			}

			void visitRoundExpr(pst::Access<pst::expr::RoundExpr> round_expr) override {
				round_expr->getInner().unlock(ctx)->acceptExprVisitor(*this);
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
					ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
					ctx.logInt(makeBox<dia::PlaceholderError>(
						"Left side of assignment can't be immutable.", stmt->getStablePosition()
					));
					return;
				}

				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		const tsh::SymbolType<>                          expected_type,
		base::Optional<dia::StablePosition>              coercion_expects_pos,
		CoercionErrorOverrides                           error_overrides
	) {
		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });

		UNPACK_QRESULT_CREF_TO_BOX(CRef<code::Expr> expr_hout =, expr_hout_qresult);

		const auto source_position = pst_expr.element.unlock(ctx)->getStablePosition();
		UNPACK_QRESULT(
			auto coercion_result =, canCoerce(ctx, expr_hout->expression_type, expected_type)
		);

		if (coercion_result.isValid()) return coercion_result.coerceFromRef(ctx, expr_hout);

		logCoercionFailure(
			ctx, coercion_result, source_position, coercion_expects_pos, std::move(error_overrides)
		);
		return query::Failed();
	}

	query::QResult<BoxOrCRef<code::Expr>> getHoutOfExprWithExpectedTypes(
		query::Context&                                  ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>& pst_expr,
		const std::vector<tsh::SymbolType<>>&            expected_types
	) {
		CORE_ASSERT(!expected_types.empty(), "At least one expected type has to be provided.");
		if (expected_types.size() == 1)
			return getHoutOfExprWithExpectedType(ctx, pst_expr, expected_types.front());

		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });

		UNPACK_QRESULT_CREF_TO_BOX(CRef<code::Expr> expr_hout =, expr_hout_qresult);

		std::vector<Coercion> failures;
		failures.reserve(expected_types.size());

		for (const tsh::SymbolType<>& expected_type: expected_types) {
			const auto coercion_qresult = canCoerce(ctx, expr_hout->expression_type, expected_type);
			if (coercion_qresult.hasFailed()) return query::Failed();

			const auto& coercion_result = coercion_qresult.valueOrThrow();
			if (coercion_result.isValid()) return coercion_result.coerceFromRef(ctx, expr_hout);

			failures.push_back(coercion_result);
		}

		logNoMatchingExpectedTypeFailure(
			ctx, failures, pst_expr.element.unlock(ctx)->getStablePosition()
		);
		return query::Failed();
	}

	query::QResult<Box<code::Expr>> code::subExprFromPSTWithType(
		query::Context&                     ctx,
		pst::AccessLocked<pst::ExprElement> element,
		tsh::SymbolType<>                   expected_type,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		helios::CoercionErrorOverrides      error_overrides
	) {
		auto expr_hout_qresult = code::subExprFromPST(ctx, element);
		UNPACK_QRESULT_MOVE(auto expr_hout =, expr_hout_qresult);
		const auto source_position = element.unlock(ctx)->getStablePosition();

		auto maybe_coerced = coerceFromBox(
			ctx,
			std::move(expr_hout),
			expected_type,
			source_position,
			coercion_expects_pos,
			std::move(error_overrides)
		);
		if (maybe_coerced.has_value()) return std::move(maybe_coerced.value());
		return query::Failed();
	}
}
