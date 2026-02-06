#include "query_hout_of_expr.hpp"

#include "coercions.hpp"
#include "errors.hpp"
#include "numeric_literals.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/expressions/builtin_operations.hpp>
#include <helios_private/expressions/chain_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <typesystem/higher/queries.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

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
			UnknownEscapeSequenceError(dia::SourcePosition source_position, std::string sequence):
				  MessageWithCodeFragmentAndCause(source_position) {
				addArgument<dia_int::TextArgument>("sequence", std::move(sequence));
				addAttachedMessage(makeBox<SupportedEscapeSequencesDocs>());
			}
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
				if (bin_op->getOperator()
				    == lang_def::operatorToStr(lang_def::NamedOperator::Pipe)) {
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
				expr.unlock(ctx)->getOperator()
					== lang_def::operatorToStr(lang_def::NamedOperator::Pipe),
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
			base::Optional<base::Box<Expr>> node;

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

			void visitExprStrValue(pst::Access<pst::expr::ExprStrValue> stmt) override {
				const auto escaped_string  = stmt->getValue().value.strView();
				const auto unescape_result = base::unescapeString(escaped_string);
				variant_match(unescape_result) {
					variant_case(base::UnescapedString, result) {
						node = makeBox<LiteralStringExpr>(
							ctx, pstOrigin(stmt), base::StrID(result.value)
						);
					}
					variant_case(base::UnknownEscapeSequence, error) {
						ctx.logInt(makeBox<UnknownEscapeSequenceError>(
							stmt->getSourcePosition(), error.value
						));
					}
					variant_default CORE_UNREACHABLE();
				}
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> binaryBuiltin(
				pst::Access<pst::expr::BinaryOperator> expr, Box<Expr> lhs, Box<Expr> rhs
			) {
				auto result = findBinaryBuiltin(ctx, expr->getOperator(), lhs.ref(), rhs.ref());

				match_optional(result) {
					opt_some_move(value) {
						auto [operation, lhs_coercion, rhs_coercion] = value;

						auto coerced_lhs = lhs_coercion.coerce(ctx, std::move(lhs));
						auto coerced_rhs = rhs_coercion.coerce(ctx, std::move(rhs));

						return makeBox<BinaryOperatorExpr>(
							ctx,
							pstOrigin(expr),
							operation,
							std::move(coerced_lhs),
							std::move(coerced_rhs)
						);
					}
					opt_none { return {}; }
				}
				return {};
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
							std::move(origin), operation, std::move(coerced)
						);
					}
					opt_none { return {}; }
				}
				CORE_UNREACHABLE();
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// handle variants:
				if (stmt->getOperator() == lang_def::operatorToStr(lang_def::NamedOperator::Pipe)) {
					auto                   sub_exprs = getVariantSubExprs(ctx, stmt);
					std::vector<Box<Expr>> all_subtypes;

					// Expect all subexpressions in variant constructor to be Meta types or try to
					// lift them if they aren't.
					const auto meta_type = tsh::SymbolType<>{
						ctx.query<tsh::QueryMetaType>({}),
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

				auto lhs_res = fromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt->getRightOperand());

				// @todo: make failure more explicit...
				if (lhs_res.hasFailed() or rhs_res.hasFailed()) return;  // failed

				auto lhs = std::move(lhs_res).valueOrThrow();
				auto rhs = std::move(rhs_res).valueOrThrow();

				auto lhs_type = lhs->expression_type.getSymbolType();
				auto rhs_type = rhs->expression_type.getSymbolType();

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				auto builtin = binaryBuiltin(stmt, std::move(lhs), std::move(rhs));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.logInt(makeBox<code::UndefinedBinaryOperatorError>(
						stmt->getSourcePosition(),
						stmt->getOperator().str(),
						makeBox<InteractiveType>(ctx, lhs_type),
						makeBox<InteractiveType>(ctx, rhs_type)
					));
					// failed
				}
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
				// note: this is a mock, it should be unified with ChainExpr
				auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

				const auto& sym_list = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
					stmt->getName().position, ctx, stmt->getName().value
				);

				node
					= makeBox<IdentifierExpr>(ctx, pstOrigin(stmt), sym_list.valueOrThrow().back());
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
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryBoolType>({})
					);
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryCharType>({})
					);
					break;

				case pst::Keyword::Str:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryStringType>({})
					);
					break;

				case pst::Keyword::Type:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryMetaType>({})
					);
					break;

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 128, Signed })
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 64, Signed })
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 32, Signed })
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 16, Signed })
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 8, Signed })
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 128, Unsigned })
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 64, Unsigned })
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 32, Unsigned })
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 16, Unsigned })
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryIntegralType>({ 8, Unsigned })
					);
					break;

				case pst::Keyword::f80:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryFloatType>({ 80 })
					);
					break;
				case pst::Keyword::f128:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryFloatType>({ 128 })
					);
					break;
				case pst::Keyword::f64:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryFloatType>({ 64 })
					);
					break;
				case pst::Keyword::f32:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryFloatType>({ 32 })
					);
					break;
				case pst::Keyword::f16:
					node = makeBox<LiteralTypeExpr>(
						ctx, pstOrigin(stmt), ctx.query<tsh::QueryFloatType>({ 16 })
					);
					break;


				default:
					CORE_PANIC(
						"Keyword not yet handled (or bad keyword) by PstExprToHoutExprVisitor"
					);
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

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator>) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				throw base::NotYetImplemented(
					"Suffix operators are not yet implemented in HOUT, since there are any for "
					"now"
				);
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

				if (stmt->getOperator()
				    == lang_def::operatorToStr(lang_def::NamedOperator::Ampersand)) {
					// @TODO: #1549 RefOfExpr is inserted here naively without any checks.
					// This should change to take value category into consideration as well as
					// the `unique`/`leaking` specifiers.
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
				auto condition_res = fromPST(ctx, stmt->getCondition());
				auto if_true_res   = fromPST(ctx, stmt->getIfTrue());
				auto if_false_res  = fromPST(ctx, stmt->getIfFalse());

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
				size_t      operator_count = std::ranges::size(pst_operators);
				size_t      expr_count     = operator_count + 1;

				std::vector<Box<Expr>> result_exprs;
				result_exprs.reserve(expr_count);
				for (size_t i = 0; i < expr_count; ++i) {
					auto result = fromPST(ctx, stmt->getSubExpr(i));
					if (result.hasFailed())
						return;
					else
						result_exprs.push_back(std::move(result.valueOrThrow()));
				}

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				std::vector<BuiltinBinary> operators;
				operators.reserve(operator_count);
				for (size_t i = 0; i < operator_count; ++i) {
					auto lhs_type = result_exprs.at(i)->expression_type.getSymbolType();
					auto rhs_type = result_exprs.at(i + 1)->expression_type.getSymbolType();

					auto result = findBinaryBuiltin(
						ctx,
						pst_operators.at(i),
						result_exprs.at(i).ref(),
						result_exprs.at(i + 1).ref()
					);

					if (result) {
						auto [op, lhs_coercion, rhs_coercion] = std::move(result).value();
						result_exprs[i] = lhs_coercion.coerce(ctx, std::move(result_exprs[i]));
						result_exprs[i + 1]
							= rhs_coercion.coerce(ctx, std::move(result_exprs[i + 1]));
						operators.push_back(op);
					} else {
						ctx.logInt(makeBox<code::UndefinedBinaryOperatorError>(
							stmt->getSourcePosition(),
							pst_operators.at(i).str(),
							makeBox<InteractiveType>(ctx, lhs_type),
							makeBox<InteractiveType>(ctx, rhs_type)
						));

						return;
					}
				}

				node = makeBox<ChainComparisonExpr>(
					ctx, pstOrigin(stmt), std::move(result_exprs), std::move(operators)
				);
			}
		};

		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		) {
			PstExprToHoutExprVisitor visitor(ctx);
			element.unlock(ctx)->acceptExprVisitor(visitor);

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

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr)

	ExprConstructionResult getHoutOfExprWithExpectedType(
		query::Context&                                      ctx,
		const pst::GenericPSTQueryKey<pst::ExprElement>&     pst_expr,
		const tsh::SymbolType<>                              expected_type,
		base::Optional<std::function<void(query::Context&)>> log_error
	) {
		auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr.element });

		if (expr_hout_qresult.hasFailed()) return query::Failed();

		auto expr_hout = std::move(expr_hout_qresult).valueOrThrow();

		const auto coercion_qresult
			= canCoerce(ctx, expr_hout->expression_type.getSymbolType(), expected_type);
		if (coercion_qresult.hasFailed()) return query::Failed();

		variant_match(coercion_qresult.valueOrThrow().getVariant()) {
			variant_case(Coercion, coercion) { return coercion.coerce(ctx, std::move(expr_hout)); }
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
			variant_default { CORE_PANIC("Unhandled coercion result variant."); }
		}
		CORE_UNREACHABLE();
	}
}
