#include "query_hout_of_expr.hpp"

#include "coercions.hpp"
#include "errors.hpp"
#include "function_calls/call_processing.hpp"
#include "numeric_literals.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/expressions/builtin_operators.hpp>
#include <helios_private/hout_creation/expressions/chain_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

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
			UnknownEscapeSequenceError(
				const dia::SourcePosition& source_position, std::string sequence
			):
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
			explicit InvalidCharacterLiteralError(const dia::SourcePosition& source_position):
				  MessageWithCodeFragment(source_position) {}
		};

		/**
		 * This is an effective implementation of QueryHoutOfExpr.
		 * QueryHoutOfExpr is mostly a wrapper for future cache.
		 * @note This is a private function of this file.
		 */
		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		);

		void getVariantSubExprsInPlace(
			query::Context&                                   ctx,
			pst::AccessLocked<pst::ExprElement>               expr,
			std::vector<pst::AccessLocked<pst::ExprElement>>& sub_exprs_append
		) {
			if (auto bin_op_opt = expr.unlock(ctx).dynamicCast<pst::expr::BinaryOperator>()) {
				auto bin_op = bin_op_opt.value();
				if (bin_op->getOperator() == lang_def::NamedOperator::Pipe) {
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
				expr.unlock(ctx)->getOperator() == lang_def::NamedOperator::Pipe,
				"Not a variant operator"
			);
			std::vector<pst::AccessLocked<pst::ExprElement>> sub_exprs;
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getLeftOperand(), sub_exprs);
			getVariantSubExprsInPlace(ctx, expr.unlock(ctx)->getRightOperand(), sub_exprs);
			return sub_exprs;
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
								makeBox<InvalidCharacterLiteralError>(stmt->getSourcePosition())
							);
						}
					}
					opt_err(error) {
						ctx.logInt(makeBox<UnknownEscapeSequenceError>(
							stmt->getSourcePosition(), error.value
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
							stmt->getSourcePosition(), error.value
						));
					}
				}
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
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> unaryBuiltin(
				lexer::Operator op, Box<Expr> expr, ElementOrigin origin
			) {
				auto result = findUnaryBuiltin(ctx, op, expr.ref());
				match_optional(result) {
					opt_some_move(value) {
						auto [operation, coercion] = value;
						auto coerced               = coercion.coerce(ctx, std::move(expr));
						return makeBox<UnaryOperatorExpr>(
							ctx, origin, operation, std::move(coerced)
						);
					}
					opt_none { return {}; }
				}
				CORE_UNREACHABLE();
			}

			/**
			 * @brief Finds the appropriate binary operator to call and constructs the corresponding
			 * HOUT expression. Consumes the provided expressions of the arguments.
			 * @param op The operator
			 * @param lhs The precomputed left-hand side argument
			 * @param rhs The precomputed right-hand side argument
			 * @param scope The scope in which the operator call happens
			 */
			[[nodiscard]]
			Box<Expr> resolveBinaryOperator(
				lexer::Operator op, Box<Expr> lhs, Box<Expr> rhs, ScopeID scope
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
				    && isNumericOperator(op)) {
					auto numeric_builtin_opt
						= findNumericBinaryBuiltin(ctx, op, lhs.ref(), rhs.ref());
					auto new_origin = elementOrigin(lhs->origin, rhs->origin);

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
					= HInterface::ofScopeWithParents(scope).lookup(ctx, op.value);
				// @TODO: #1412 fix dealias
				auto all_candidates = lookup_result->valueOrThrow().leaves;
				for (const auto [builtin_operator_sym, _]:
				     *ctx.query<QueryRegularBinaryBuiltinSymbols>({})) {
					if (name(builtin_operator_sym) == op.value)
						all_candidates.push_back(builtin_operator_sym);
				}
				return processBinaryOperatorCall(ctx, all_candidates, std::move(lhs), std::move(rhs))
				    .valueOrThrow();
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// handle variants:
				const auto op = stmt->getOperator();
				if (op == lang_def::NamedOperator::Pipe) {
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
						auto sub_expr_hout
							= getHoutOfExprWithExpectedType(ctx, sub_expr, meta_type);
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
				auto lhs_res = fromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt->getRightOperand());

				auto lhs = std::move(lhs_res).valueOrThrow();
				auto rhs = std::move(rhs_res).valueOrThrow();

				node = resolveBinaryOperator(
					op, std::move(lhs), std::move(rhs), ctx.query<QueryPrimaryCodeScopeFor>({ stmt })
				);
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
				switch (stmt->getKeyword()) {
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
						stmt->getSourcePosition(), ctx, base::StrID("self")
					);

					node = makeBox<IdentifierExpr>(
						ctx, pstOrigin(stmt), sym_list.valueOrThrow().back()
					);
					break;
				}
				default:
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Keyword not yet handled.", stmt->getSourcePosition()
					));
					return;  // failed
				}
			}

			void visitComma(pst::Access<pst::expr::Comma> stmt) override {
				std::vector<Box<Expr>> expressions;
				for (auto ex: stmt->getExpressions()) {
					auto res = fromPST(ctx, ex);
					if (res.hasFailed()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).valueOrThrow());
				}
				node = makeBox<TupleExpr>(ctx, pstOrigin(stmt), std::move(expressions));
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator> stmt) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Suffix operators are not implemented yet in HOUT, since they don't exist "
					"yet.",
					stmt->getSourcePosition()
				));
				return;  // failed
			}

			void visitPrefixOperator(pst::Access<pst::expr::PrefixOperator> stmt) override {
				// @NOTE: This is a mockup
				auto inner_res = fromPST(ctx, stmt->getExpr());
				if (inner_res.hasFailed()) return;  // failed

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:
				auto inner      = std::move(inner_res).valueOrThrow();
				auto inner_type = inner->expression_type.getSymbolType();

				if (stmt->getOperator() == lang_def::NamedOperator::Ampersand) {
					// @TODO: #1956 remove the check bellow.
					// This is a temporary check to prevent us from taking reference of types that
					// do not carry information.
					if (not inner_type.getType().carriesInformation(ctx)) {
						ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
							"Taking reference of type that does not carry information is not "
							"supported yet.",
							stmt->getSourcePosition()
						));
						return;  // failed
					}
					// @TODO: #1549 RefOfExpr is inserted here naively without any checks.
					// This should change to take value category into consideration as well as the
					// `unique`/`leaking` specifiers.
					auto primary_category = inner->expression_type.getValueCategory().getCategory();
					if (primary_category == tsh::PrimaryCategory::Literal
					    || primary_category == tsh::PrimaryCategory::Temporary) {
						ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
							"Tried to reference a temporary", stmt->getSourcePosition()
						));
						return;
					}
					node = makeBox<RefOfExpr>(ctx, pstOrigin(stmt), std::move(inner));
					return;
				}

				auto builtin = unaryBuiltin(stmt->getOperator(), std::move(inner), pstOrigin(stmt));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.logInt(makeBox<UndefinedUnaryOperatorError>(
						stmt->getSourcePosition(),
						stmt->getOperator().str(),
						makeBox<InteractiveType>(ctx, inner_type)
					));
					// failed
				}
			}

			void visitTernary(pst::Access<pst::expr::Ternary> stmt) override {
				const auto bool_type = tsh::SymbolType<>::withDefaults(tsh::getBoolType());
				// @TODO: #2063 Change that to subExprFromPSTWithType :)
				auto condition_res
					= getHoutOfExprWithExpectedType(ctx, stmt->getCondition(), bool_type);

				auto if_true_res  = fromPST(ctx, stmt->getIfTrue());
				auto if_false_res = fromPST(ctx, stmt->getIfFalse());

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

				const auto& pst_operators  = stmt->getOperators();
				auto        operator_count = usize(std::ranges::size(pst_operators));
				usize       expr_count     = operator_count + 1;

				std::vector<Box<Expr>> result_exprs;
				result_exprs.reserve(expr_count);
				for (usize i = 0; i < expr_count; ++i) {
					auto result = fromPST(ctx, stmt->getSubExpr(i));
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
					const auto op = pst_operators.at(op_idx);
					auto rhs = makeBox<ReusableExpr>(ctx, std::move(result_exprs.at(op_idx + 1)));
					auto next_lhs = rhs->nextUse();

					comparisons.emplace_back(
						resolveBinaryOperator(op, std::move(lhs), std::move(rhs), scope)
					);
					lhs = std::move(next_lhs);
				}

				node = makeBox<ChainComparisonExpr>(ctx, pstOrigin(stmt), std::move(comparisons));
			}
		};

		ExprConstructionResult fromPST(
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
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO static assert this is top-expr
			return code::fromPST(ctx, key.element);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	ExprConstructionResult getHoutOfExprWithExpectedType(
		query::Context&                                      ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>&     pst_expr,
		const tsh::SymbolType<>                              expected_type,
		base::Optional<std::function<void(query::Context&)>> log_error
	) {
		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });

		UNPACK_QRESULT_CREF_TO_BOX(auto expr_hout =, expr_hout_qresult);

		const auto coercion_qresult
			= canCoerce(ctx, expr_hout->expression_type.getSymbolType(), expected_type);
		if (coercion_qresult.hasFailed()) return query::Failed();

		variant_match(coercion_qresult.valueOrThrow().getVariant()) {
			variant_case(Coercion, coercion) { return coercion.coerce(ctx, expr_hout->clone()); }
			variant_case(InvalidCoercion, _) {
				if (log_error.has_value()) {
					(*log_error)(ctx);
				} else {
					ctx.logInt(makeBox<IncompatibleTypesError>(
						pst_expr.element.unlock(ctx)->getSourcePosition(),
						makeBox<InteractiveType>(ctx, expr_hout->expression_type.getSymbolType()),
						makeBox<InteractiveType>(ctx, expected_type)
					));
				}
				return query::Failed();
			}
			variant_case(TypeNotTriviallyCopyable, _) {
				if (expr_hout->expression_type.getSymbolType().getRefKind()
				        == tsh::ReferenceKind::Ref
				    && expected_type.getRefKind() == tsh::ReferenceKind::Direct) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"Copy constructor for non-trivially-copyable type `",
							expr_hout->expression_type.getSymbolType()
								.withReferenceKind(tsh::ReferenceKind::Direct)
								.toString(),
							"`. This was caused by the need of dereferencing a value of type: "
							"`",
							expr_hout->expression_type.getSymbolType().toString(),
							"`."
						),
						pst_expr.element.unlock(ctx)->getSourcePosition()
					));

				} else {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"Copy constructor for non-trivially-copyable type `",
							expr_hout->expression_type.getSymbolType().toString(),
							"`."
						),
						pst_expr.element.unlock(ctx)->getSourcePosition()
					));
				}
				return query::Failed();
			}
			variant_default { CORE_PANIC("Unhandled coercion result variant."); }
		}
		CORE_UNREACHABLE();
	}
}
