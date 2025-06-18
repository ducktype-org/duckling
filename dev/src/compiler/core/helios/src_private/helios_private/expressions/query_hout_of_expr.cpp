#include "query_hout_of_expr.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <helios_private/expressions/builtin_operations.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <pst_parser/pst_expr_visitor.hpp>
#include <query_framework/query_error.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/higher/queries.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>

namespace compiler::helios::code {
	namespace {

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
				if (bin_op->getOperator().str() == "|") {
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
			CORE_ASSERT(expr.unlock(ctx)->getOperator().str() == "|", "Not a variant operator");
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

			void visitExprValue(pst::Access<pst::expr::ExprValue> stmt) override {
				// @TODO: Change literal value from i64 to something more appropriate.
				node = makeBox<LiteralIntExpr>(ctx, std::stoi(stmt->getValue().str()));
			}

			void visitExprStrValue(pst::Access<pst::expr::ExprStrValue> stmt) override {
				node = makeBox<LiteralStringExpr>(ctx, stmt->getValue());
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> binaryBuiltin(
				lexer::Operator op, Box<Expr> lhs, Box<Expr> rhs
			) {
				auto operation = findBinaryBuiltin(op, lhs.ref(), rhs.ref());
				if (operation) {
					return makeBox<BinaryOperatorExpr>(
						ctx, operation.value(), std::move(lhs), std::move(rhs)
					);
				}
				return {};
			}

			/**
			 * If a valid builtin exists (special characters only), returns it.
			 * Otherwise, returns None.
			 */
			base::Optional<Box<Expr>> unaryBuiltin(lexer::Operator op, Box<Expr> expr) {
				auto operation = findUnaryBuiltin(op, expr.ref());

				if (operation)
					return makeBox<UnaryOperatorExpr>(operation.value(), std::move(expr));
				else
					return {};
			}

			void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> stmt) override {
				// handle variants:
				if (stmt->getOperator().str() == "|") {
					// @todo HOUT 2.0:
					// Here we assume that "|" always produces a variant (likely valid).
					// If it does not, and "|" will remain a binary operator,
					// we will have to do something with it.
					// (likely if-out if all sub expressions are meta or non-meta, throw otherwise,
					// (require parentheses))

					auto sub_exprs = getVariantSubExprs(ctx, stmt);
					// @todo HOUT 2.0:
					// validate that all sub types are meta

					std::vector<Box<Expr>> all_subtypes;

					for (auto sub_expr: sub_exprs) {
						auto sub_expr_hout = fromPST(ctx, sub_expr);
						if (sub_expr_hout.hasError()) {
							// Error has occurred.
							return;
						}
						all_subtypes.emplace_back(std::move(sub_expr_hout).value());
					}
					node = makeBox<VariantTypeConstructorExpr>(ctx, std::move(all_subtypes));
					return;
				}

				auto lhs_res = fromPST(ctx, stmt->getLeftOperand());
				auto rhs_res = fromPST(ctx, stmt->getRightOperand());

				// @todo: make failure more explicit...
				if (lhs_res.hasError() or rhs_res.hasError()) return;  // failed

				auto lhs = std::move(lhs_res).value();
				auto rhs = std::move(rhs_res).value();

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto builtin = binaryBuiltin(stmt->getOperator(), std::move(lhs), std::move(rhs));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
							stmt->getSourcePosition(), "No builtin operator found"
						)
					);
					// failed
				}
			}

			void visitChainExpr(pst::Access<pst::expr::ChainExpr> stmt) override {
				// @TODO: Add a compiler log or some kind of information if lookup fails.
				// @TODO / @NOTE This methods will be reworked.
				// @TODO: add compiler errors, typecheck and coercions in function calls

				auto atom_expr = fromPST(ctx, stmt->getAtom());
				if (!atom_expr) {
					// Report an error?
					return;
				}

				// this is very temporary:
				SymbolList looked_up_symbol
					= { { querySymIDOfHOUTExpr(ctx, atom_expr.value().ref()).value() } };

				// @note If optional is not empty it means there has been a call.
				base::Optional<std::vector<Box<Expr>>> call_arguments;

				for (auto el: stmt->getChain()) {
					// This is a mock. It asserts call expression is the last in the chain.
					if (call_arguments.has_value())
						throw base::NotYetImplemented("Call expr not last on the chain");
					if (auto pst_access_opt = el.unlock(ctx).dynamicCast<pst::expr::Access>()) {
						auto pst_access = pst_access_opt.value();
						CORE_ASSERT(
							pst_access->getType() == ".", "Not handling .? access operator yet"
						);

						std::cerr << "Lookup in: " << name(looked_up_symbol.back()).strView() << " "
								  << pst_access->getName().value.strView() << "\n";

						auto new_symbols = HInterface::ofSymbol(looked_up_symbol.back())
						                       .lookupExpectUnique(
												   pst_access->getName().position,
												   ctx,
												   pst_access->getName().value
											   );

						if (!new_symbols) return;  // failed

						looked_up_symbol.appendList(new_symbols.value());
					} else if (auto pst_call_opt = el.unlock(ctx).dynamicCast<pst::expr::Call>()) {
						auto pst_call = pst_call_opt.value();
						if (pst_call->getType() != lexer::Token::Round) {
							throw base::NotYetImplemented(base::strConcat(
								"HOUT call with invalid bracket type: ", char(pst_call->getType())
							));
						}

						call_arguments.emplace();
						for (auto&& arg: *pst_call->getArgs().unlock(ctx)) {
							auto arg_expr = fromPST(ctx, arg.unlock(ctx)->getExpr());
							if (!arg_expr) {
								// Error
								return;
							}
							call_arguments->emplace_back(std::move(arg_expr.value()));
						}
					} else {
						throw base::NotYetImplemented(
							"ChainExpr visitor handles only calls and accesses."
						);
					}
				}

				if (call_arguments) {
					auto call_symbol      = looked_up_symbol.back();
					auto call_type_result = ctx.query<QueryTypeOfSymbol>({ call_symbol });
					if (call_type_result->hasError()) return;  // fail
					tsh::SymbolType<tsh::FunctionAbstractType> call_type
						= call_type_result->value();

					auto arguments = std::move(call_arguments.value());
					if (call_type.getType().getParameterTypes().size() != arguments.size()) {
						ctx.log(makeBox<
								dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							stmt->getSourcePosition(), "Invalid number of arguments"
						));
						return;  // failed
					}

					std::vector<base::Box<Expr>> coerced_arguments;
					coerced_arguments.reserve(arguments.size());
					for (usize i = 0; i < arguments.size(); i++) {
						auto coerced = coerceExpression(
							std::move(arguments[i]), call_type.getType().getParameterTypes()[i]
						);
						if (coerced.hasError()) {
							// this has suboptimal error position,
							// for now it is left like this, since
							// this function will be reworked anyway:
							ctx.log(makeBox<dia::PlaceholderMessage<
										dia::Error,
										dia::Message::Domain::TypeCheck>>(
								stmt->getSourcePosition(), "Invalid argument type"
							));
							return;  // failed
						}
						coerced_arguments.emplace_back(std::move(coerced.value()));
					}


					CORE_ASSERT(
						coerced_arguments.size() == call_type.getType().getParameterTypes().size(),
						"Invalid number of arguments after type check"
					);

					node = makeBox<CallExpr>(ctx, call_symbol, std::move(coerced_arguments));
				} else {
					node = makeBox<LinkedIdentifierExpr>(ctx, std::move(looked_up_symbol));
				}
			}

			void visitRoundExpr(pst::Access<pst::expr::RoundExpr> stmt) override {
				PstExprToHoutExprVisitor vis(ctx);
				stmt->getInner().unlock(ctx)->acceptExprVisitor(vis);
				if (vis.node) node = makeBox<ParenthesisExpr>(ctx, std::move(*vis.node));
			}

			void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> stmt) override {
				// note: this is a mock, it should be unified with ChainExpr
				auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

				const auto& sym_list = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
					stmt->getName().position, ctx, stmt->getName().value
				);
				if (!sym_list) {
					// failed
					return;
				}

				node = makeBox<IdentifierExpr>(ctx, sym_list.value().back());
			}

			void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> stmt) override {
				using enum tsh::IntegralAbstractType::Signedness;
				switch (stmt->getKeyword()) {
				// true, false:
				case pst::Keyword::True:
					node = makeBox<LiteralBoolExpr>(ctx, true);
					break;
				case pst::Keyword::False:
					node = makeBox<LiteralBoolExpr>(ctx, false);
					break;


				// types:
				case pst::Keyword::Bool:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryBoolType>({}));
					break;

				case pst::Keyword::Char:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryCharType>({}));
					break;

				case pst::Keyword::Str:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryStringType>({}));
					break;

					// @todo: add meta keyword and type

				case pst::Keyword::i128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Signed })
					);
					break;
				case pst::Keyword::i64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Signed })
					);
					break;
				case pst::Keyword::i32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Signed })
					);
					break;
				case pst::Keyword::i16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Signed })
					);
					break;
				case pst::Keyword::i8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Signed })
					);
					break;

				case pst::Keyword::u128:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 128, Unsigned })
					);
					break;
				case pst::Keyword::u64:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 64, Unsigned })
					);
					break;
				case pst::Keyword::u32:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 32, Unsigned })
					);
					break;
				case pst::Keyword::u16:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 16, Unsigned })
					);
					break;
				case pst::Keyword::u8:
					node = makeBox<LiteralTypeExpr>(
						ctx, ctx.query<tsh::QueryIntegralType>({ 8, Unsigned })
					);
					break;

				case pst::Keyword::f80:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(80));
					break;
				case pst::Keyword::f128:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(128));
					break;
				case pst::Keyword::f64:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(64));
					break;
				case pst::Keyword::f32:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(32));
					break;
				case pst::Keyword::f16:
					node = makeBox<LiteralTypeExpr>(ctx, ctx.query<tsh::QueryFloatType>(16));
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
					if (res.hasError()) {
						// Error has occurred.
						return;
					}
					expressions.emplace_back(std::move(res).value());
				}

				node = makeBox<TupleTypeConstructorExpr>(ctx, std::move(expressions));
			}

			void visitSuffixOperator(pst::Access<pst::expr::SuffixOperator>) override {
				// note: here we will have to compile things like `a++`, `a--`, `T?`.
				throw base::NotYetImplemented(
					"Suffix operators are not yet implemented in HOUT, since there are any for now"
				);
			}

			void visitPrefixOperator(pst::Access<pst::expr::PrefixOperator> stmt) override {
				// @NOTE: This is a mockup
				auto inner = fromPST(ctx, stmt->getExpr());
				if (inner.hasError()) return;  // failed

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto builtin = unaryBuiltin(stmt->getOperator(), std::move(inner.value()));

				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
							stmt->getSourcePosition(), "No builtin operator found"
						)
					);
					// failed
				}
			}

			void visitTernary(pst::Access<pst::expr::Ternary> stmt) override {
				auto condition_res = fromPST(ctx, stmt->getCondition());
				auto if_true_res   = fromPST(ctx, stmt->getIfTrue());
				auto if_false_res  = fromPST(ctx, stmt->getIfFalse());

				if (condition_res.hasError() or if_true_res.hasError() or if_false_res.hasError())
					return;

				auto condition = std::move(condition_res).value();
				auto if_true   = std::move(if_true_res).value();
				auto if_false  = std::move(if_false_res).value();

				node = makeBox<TernaryOperatorExpr>(
					ctx, std::move(condition), std::move(if_true), std::move(if_false)
				);
			}

			void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> stmt) override {
				// for now we only compile chains of length 1 (i.e. not chains).

				CORE_ASSERT(stmt->getOperators().size() == 1, "Not a chain of length 1");

				auto lhs_res = fromPST(ctx, stmt->getSubExpr(0));
				auto rhs_res = fromPST(ctx, stmt->getSubExpr(1));

				if (lhs_res.hasError() or rhs_res.hasError()) return;  // failed

				auto lhs = std::move(lhs_res).value();
				auto rhs = std::move(rhs_res).value();

				// @todo here we should:
				// * lookup for user defined operators
				// * type check
				// * make function call
				// For now we support just builtins

				// if no function call is found, we try to use builtin operators:

				auto builtin
					= binaryBuiltin(stmt->getOperators().at(0), std::move(lhs), std::move(rhs));
				if (builtin.has_value()) {
					node = std::move(builtin).value();
					return;
				} else {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
							stmt->getSourcePosition(), "No builtin operator found"
						)
					);
					// failed
				}
			}
		};

		ExprConstructionResult fromPST(
			query::Context& ctx, pst::AccessLocked<pst::ExprElement> element
		) {
			// std::cerr << "\nExpr: \n";
			// root->debugPrint(std::cerr);
			// std::cerr << '\n'

			PstExprToHoutExprVisitor visitor(ctx);
			element.unlock(ctx)->acceptExprVisitor(visitor);

			if_opt_some(visitor.node, expr) return std::move(expr);
			return query::QError(errors::Failed());
		}
	}
}

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryHoutOfExpr, ExprConstructionResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// Note: we might actually accept nulls in such queries, and just return failed
			// Something to think about as part of #412
			CORE_ASSERT(
				key.element.unlockOpt(ctx).has_value(), "Nullptr provided to QueryHoutOfExpr"
			);

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
}
