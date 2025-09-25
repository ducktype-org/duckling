/**
 * @file chain_expr.cpp
 * @author Wojciech Rzepliński
 */

#include "chain_expr.hpp"

#include "call_processing.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <typesystem/higher/types.hpp>

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>
#include <token_parser_core/common_elements.hpp>

namespace compiler::helios::code {

	/**
	 * State for the building of the chain expression in HOUT.
	 * It mainly stores two distinct states:
	 * - namespace-like state, for example after processing "foo().Namespace"
	 * - expression state, for example after processing "foo().bar[20]"
	 *
	 * Its stores the state of chain expression creation that does not include
	 * all previously cut-off expressions.
	 */
	struct ChainState final {
		[[nodiscard]] bool isNamespaceLike() const { return namespace_like_symbol.has_value(); }

		[[nodiscard]] bool isExpr() const { return expr.toOpt().has_value(); }

		[[nodiscard]] bool isEmpty() const { return not isNamespaceLike() and not isExpr(); }

		[[nodiscard]] auto getExpr() -> base::Box<Expr> {
			return std::move(expr).toOptBox().value();
		}

		[[nodiscard]] auto getNamespaceLikeSymbol() -> SymID {
			return namespace_like_symbol.value();
		}

		static ChainState ofExpr(base::Box<Expr> expr) { return { std::move(expr) }; }

		static ChainState ofNamespaceLike(SymID namespace_like_symbol) {
			return { namespace_like_symbol };
		}

	private:
		ChainState(): expr(base::MBox<Expr>{}), namespace_like_symbol(base::Optional<SymID>{}) {}

		ChainState(base::Box<Expr> expr):
			  expr(std::move(expr)),
			  namespace_like_symbol(base::Optional<SymID>{}) {}

		ChainState(SymID namespace_like_symbol):
			  expr(base::MBox<Expr>{}),
			  namespace_like_symbol(namespace_like_symbol) {}

		base::MBox<Expr>      expr{};
		base::Optional<SymID> namespace_like_symbol{};

		friend class ChainExprConstruction;
	};

	/**
	 * Constructs an expression from a PST chain expression, for example from
	 * "a.b.c().d[20].W". It processes the chain from left to right, building the expression step by
	 * step. The state of the building process is stored in value of class @p ChainState.
	 *
	 * It runs in a "for-like" loop and looks for the type of the next chain element.
	 * The next chain element can be either the access expression or call expression.
	 *
	 * Based on the type of the next chain element and the current state of the chain,
	 * it finds the correct overload of the @p processPSTExpr function to call.
	 * For example where the state is "a namespace" and the next chain element is "an access",
	 * it will call @p processPSTExpr with "namespace state" and "access expression".
	 *
	 * It is like that to separate the logic of processing the chain elements and the logic of
	 * lookup and building the expression.
	 *
	 * Some processing functions expects not one but two chain elements to process:
	 * "access element" and "call element" in a row.
	 * The main processing loop ensures
	 * that when this combination of elements is found, it will call this handler.
	 * In consequence when other handlers are called it is guaranteed that
	 * before the "call element" there is no "access element" and after the "access element"
	 * there is no "call element".
	 *
	 * @p processPSTExpr - can append to result_sequence and return a new processing state
	 * @p step - based on the current state calls proper overload of @p processPSTExpr and
	 * saves the returned state in the @p current_state object member
	 * @p firstStep - same as before, but only for the first element in the chain (when state is
	 * empty)
	 * @p run - main function that loops over pst elements and calls @p step or @p firstStep
	 *
	 * @note It can also return a identifier expression with a namespace, since "(NS.NS2).a" is a
	 * thing in our compiler (namespace is a valid type that can be for example passed to a
	 * template).
	 */
	class ChainExprConstruction {
		query::Context& query_ctx;

		/**
		 * Resulting sequence of expressions.
		 * If at the end there will be a single expression, it will be the result of the chain.
		 */
		std::vector<base::Box<Expr>> result_sequence{};
		/**
		 * All chain elements that would be processed.
		 */
		std::vector<pst::AccessLocked<pst::ExprElement>> chain_elements{};

		/**
		 * The temporaty buffor for the currently built value from left to current place of
		 the chain. So for example after processing "a.b.c" it will contain hout expr:
		 "access(access(a, field=b), field=c))"".
		 */
		ChainState current_state{};
		/**
		 * this->index in the chain currently being processed.
		 */
		usize index{ 0 };

		/**
		 * Returns the current element given by the "index" object member value.
		 */
		base::Optional<pst::Access<pst::ExprElement>> currentElem() {
			if (this->index >= chain_elements.size()) return {};

			return chain_elements.at(this->index).unlock(query_ctx);
		}

		/**
		 * Returns the next element given by the "index" object member value.
		 */
		base::Optional<pst::Access<pst::ExprElement>> nextElem() {
			auto next_index = this->index + 1;
			if (next_index >= chain_elements.size()) return {};

			return chain_elements.at(next_index).unlock(query_ctx);
		}

		/**
		 * Checks if the next element is of type @p T.
		 * @tparam T The type to check against.
		 */
		template<typename T>
		bool isNextElement() {
			auto elem = nextElem();
			if (elem.empty()) return false;
			return elem.value().dynamicCast<T>().has_value();
		}

		/**
		 * Checks if the current element is of type @p T.
		 * @tparam T The type to check against.
		 */
		template<typename T>
		bool isCurrentElement() {
			auto elem = currentElem();
			if (elem.empty()) return false;
			return elem.value().dynamicCast<T>().has_value();
		}

		// =============================== MAIN PROCESSING FUNCTIONS ===============================

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain
		 * Case when we have a global identifier followed by a call expression,
		 * like "foo()".
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::IdentifierLiteral> ident, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState, errors::Failed> {
			if (call_expr->getType() != lexer::Token::Round) {
				throw base::NotYetImplemented(base::strConcat(
					"HOUT call with invalid bracket type: ", char(call_expr->getType())
				));
			}
			auto scope = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			auto lookup_result
				= HInterface::ofScopeWithParents(scope).lookup(query_ctx, ident->getName().value);

			auto res = processFunctionCall(query_ctx, lookup_result->leaves, call_expr);

			if (res.hasError()) {
				query_ctx.log(
					dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>::make(
						ident->getName().position,
						base::strConcat("Failed to find correct function: ", ident->getName().value)
					)
				);
				return query::QError(errors::Failed());
			}
			return ChainState::ofExpr(std::move(res.value()));
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * Case when as a first element we have an identifier not followed by a call expression,
		 * like "foo.bar.c".
		 */
		auto processPSTExpr(pst::Access<pst::expr::IdentifierLiteral> ident)
			-> query::QResult<ChainState, errors::Failed> {
			// Lookup global for const/variables/namespaces. Depending on the type of found
			// identifier it will return ChainContext with namespace or expr.
			auto        scope         = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			const auto& lookup_result = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
				ident->getName().position, query_ctx, ident->getName().value
			);
			if (!lookup_result) {
				query_ctx.log(
					dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::make(
						ident->getName().position,
						base::strConcat("Value '", ident->getName().value, "' not found")
					)
				);
				return query::QError(errors::Failed());
			}
			// @TODO: handle dealias expressions #981:
			const auto& sym = lookup_result.value().back();
			// @TODO: handle dealias expressions #981:
			return processNamespaceOrValue(sym);
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * Case when as a first element in the chain
		 * is a more complicated expression like (NS1.NS2).a.b.c
		 */
		auto processPSTExpr(pst::Access<pst::ExprElement> pst_expr)
			-> query::QResult<ChainState, errors::Failed> {
			auto expr = query_ctx.query<QueryHoutOfExpr>({ pst_expr });
			if (expr.hasError()) return query::QError(errors::Failed());
			auto hout_expr = std::move(expr).value();
			auto symbol    = getIdentifierExprSymID(hout_expr.ref());
			if (symbol.has_value()) return processNamespaceOrValue(symbol.value());
			return ChainState::ofExpr(std::move(hout_expr));
		}

		/**
		 * Call in situations were we don't have "access expr" then "call expr" in a row,
		 * for example we have two call expr like a[i]() or b()()
		 */
		auto processPSTExpr(base::Box<Expr> current_expr, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState, errors::Failed> {
			// @note this function is not run yet.

			// @note: previous mock-implementation of this function
			// was deleted in PR #1239. See it for reference.

			// @TODO #520 improve type lookup and provide correct
			// candidates for processFunctionCall
			// @TODO write tests for this case, when it will be implemented

			auto res = processFunctionCall(query_ctx, /* provide */ {}, call_expr);

			if (res.hasError()) return query::QError(errors::Failed());
			return ChainState::ofExpr(std::move(res.value()));
		}

		/**
		 * Call on the namespace, for example Namespace()
		 */
		auto processPSTExpr(SymID namespace_like_symbol, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState, errors::Failed> {
			(void) namespace_like_symbol;
			query_ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::make(
				call_expr->getSourcePosition(), base::strConcat("Namespace is not callable")
			));
			return query::QError(errors::Failed());
		}

		/**
		 * Case when we have an access expression, for example "x().c" or "x.y.c",
		 * not followed by a call expression.
		 * Should check if accessed field is a namespace or not.
		 */
		auto processPSTExpr(base::Box<Expr> current_expr, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<ChainState, errors::Failed> {
			// @TODO for now it is a mock as we don't have lookup in type instance and proper helios
			// access expr #520.
			auto current_expr_type = current_expr->expression_type.getType();
			auto lookup_result     = HInterface::ofTypeInstance(current_expr_type)
			                         .lookup(query_ctx, expr_access->getName().value);

			const auto& looked_up_symbols = lookup_result->getAsSingle();

			if (looked_up_symbols.hasError()) {
				auto node = makeBox<AccessExpr>(
					query_ctx, std::move(current_expr), expr_access->getName().value
				);
				return ChainState::ofExpr(std::move(node));
			} else {
				auto sym = looked_up_symbols.value().back();
				if (kind(sym) == SymbolKind::Namespace) {
					result_sequence.push_back(std::move(current_expr));
					return ChainState::ofNamespaceLike(sym);
				}
				return query::QError(errors::Failed());
			}
		}

		/**
		 * Search for value that is not called in the namespace.
		 * like "(...).NS.value"
		 */
		auto processPSTExpr(SymID namespace_like_symbol, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<ChainState, errors::Failed> {
			const auto& lookup_result
				= HInterface::ofSymbol(namespace_like_symbol)
			          .lookupExpectUnique(
						  expr_access->getSourcePosition(), query_ctx, expr_access->getName().value
					  );
			if (!lookup_result) {
				query_ctx.log(
					dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::make(
						expr_access->getName().position,
						base::strConcat("Value '", expr_access->getName().value, "' not found")
					)
				);
				return query::QError(errors::Failed());
			}
			// @TODO: handle dealias expressions #981:
			const auto& sym = lookup_result.value().back();
			return processNamespaceOrValue(sym);
		}

		/**
		 * Case when we have an access expression followed by a call expression,
		 * and current state is an expression, for example "my_expr[20].foo()".
		   This may result in a method. Method
		 * parameter overload is possible.
		 */
		auto processPSTExpr(base::Box<Expr>, pst::Access<pst::expr::Access>, pst::Access<pst::expr::Call>)
			-> query::QResult<ChainState, errors::Failed> {
			throw base::NotYetImplemented(
				"Helios chain expr: call on access expr not implemented yet"
			);
			return query::QError(errors::Failed());
		}

		/**
		 * Case when we have an access expression followed by a call expression,
		 * and current state is a namespace, for example "my_ns.foo()".
		 * This may result in function overload.
		 */
		auto processPSTExpr(
			SymID                          namespace_like_symbol,
			pst::Access<pst::expr::Access> expr_access,
			pst::Access<pst::expr::Call>   call_expr
		) -> query::QResult<ChainState, errors::Failed> {
			auto lookup_result = HInterface::ofSymbol(namespace_like_symbol)
			                         .lookup(query_ctx, expr_access->getName().value);

			// @TODO #981: make it better:
			// @TODO: #1029 handle overloads:
			auto callee = lookup_result->getAsSingle().value().back();

			auto res = processFunctionCall(query_ctx, { callee }, call_expr);
			if (res.hasError()) {
				query_ctx.log(
					dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>::make(
						expr_access->getName().position,
						base::strConcat(
							"Failed to find correct function: '", expr_access->getName().value
						)
					)
				);
				return query::QError(errors::Failed());
			}
			return ChainState::ofExpr(std::move(res.value()));
		}

		// ======================== MAIN PROCESSING FUNCTIONS HELPERS ========================


		/**
		 * Helper function of @p processPSTExpr that processes a value given the
		 * lookup result of the name.
		 */
		auto processNamespaceOrValue(const SymID& symbol)
			-> query::QResult<ChainState, errors::Failed> {
			switch (kind(symbol)) {
			case SymbolKind::Namespace:
			case SymbolKind::Import: {
				return ChainState::ofNamespaceLike(symbol);
			}
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const:
			case SymbolKind::Class: {
				auto expr = makeBox<IdentifierExpr>(query_ctx, symbol);
				return ChainState::ofExpr(std::move(expr));
			}
			default:
				return query::QError(errors::Failed());
			}
		}

		// =============================== MAIN PROCESSING LOOP ===============================

		/**
		 * Perform a procesing step on the current element of the chain.
		 * @warning It assumes that the current element exist.
		 */
		template<typename T>
		base::Optional<errors::Failed> step() {
			auto current_element_value = currentElem().value().dynamicCast<T>().value();

			auto res = [&]() -> query::QResult<ChainState, errors::Failed> {
				if (this->current_state.isExpr()) {
					auto expr = this->current_state.getExpr();
					return processPSTExpr(std::move(expr), current_element_value);
				} else if (this->current_state.isNamespaceLike()) {
					auto namespace_like_symbol = this->current_state.getNamespaceLikeSymbol();
					return processPSTExpr(namespace_like_symbol, current_element_value);
				}
				CORE_PANIC("Chain state is empty, but step() was called. This should not happen.");
			}();
			if (res.hasError()) return res.error();
			this->current_state = std::move(res.value());
			return {};
		}

		/**
		 * Perform a procesing step on current element of the chain and next one.
		 * @warning It assumes that the current element and next one exist.
		 */
		template<typename T1, typename T2>
		base::Optional<errors::Failed> step() {
			auto current_element_value = currentElem().value().dynamicCast<T1>().value();
			auto next_element_value    = nextElem().value().dynamicCast<T2>().value();
			auto res                   = [&]() -> query::QResult<ChainState, errors::Failed> {
                if (this->current_state.isExpr()) {
                    auto expr = this->current_state.getExpr();
                    return processPSTExpr(
                        std::move(expr), current_element_value, next_element_value
                    );
                } else if (this->current_state.isNamespaceLike()) {
                    auto namespace_like_symbol = this->current_state.getNamespaceLikeSymbol();
                    return processPSTExpr(
                        namespace_like_symbol, current_element_value, next_element_value
                    );
                }
                CORE_PANIC("Chain state is empty, but step() was called. This should not happen.");
			}();
			if (res.hasError()) return res.error();
			this->current_state = std::move(res.value());
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T>
		base::Optional<errors::Failed> firstStep() {
			CORE_ASSERT(
				this->current_state.isEmpty(), "Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T>().value();

			auto res = processPSTExpr(current_element_value);
			if (res.hasError()) return res.error();
			this->current_state = std::move(res.value());
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T1, typename T2>
		base::Optional<errors::Failed> firstStep() {
			CORE_ASSERT(
				this->current_state.isEmpty(), "Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T1>().value();
			auto next_element_value    = nextElem().value().dynamicCast<T2>().value();
			auto res                   = processPSTExpr(current_element_value, next_element_value);
			if (res.hasError()) return res.error();
			this->current_state = std::move(res.value());
			return {};
		}

	public:
		ChainExprConstruction(query::Context& ctx, pst::Access<pst::expr::ChainExpr> expr):
			  query_ctx(ctx) {
			this->chain_elements.emplace_back(expr->getAtom());
			auto chain = expr->getChain();
			std::ranges::copy(chain, std::back_inserter(this->chain_elements));
		}

		/**
		 * Main function of the ChainExprConstruction with the loop.
		 * Performs the construction of the chain expression from the chain elements.
		 */
		query::QResult<base::Box<Expr>, errors::Failed> run() {
			base::Optional<errors::Failed> error{};
			this->index = 0;
			if (isCurrentElement<pst::expr::IdentifierLiteral>()
			    && isNextElement<pst::expr::Call>()) {
				error = firstStep<pst::expr::IdentifierLiteral, pst::expr::Call>();
				this->index += 2;
			} else {
				error = firstStep<pst::ExprElement>();
				this->index++;
			}

			while (not error.has_value() && this->index < chain_elements.size()) {
				if (isCurrentElement<pst::expr::Access>() && isNextElement<pst::expr::Call>()) {
					error = step<pst::expr::Access, pst::expr::Call>();
					this->index += 2;  // skip next element, because it is handled
				} else if (isCurrentElement<pst::expr::Access>()) {
					error = step<pst::expr::Access>();
					this->index++;
				}

				else if (isCurrentElement<pst::expr::Call>()) {
					error = step<pst::expr::Call>();
					this->index++;
				}

				else {
					query_ctx.log(
						dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
							currentElem().value()->getSourcePosition(),
							"Expected access or call expression in chain expression"
						)
					);
					return query::QError(errors::Failed());
				}
			}
			if (error.has_value()) return query::QError(error.value());
			if (this->current_state.isExpr())
				result_sequence.push_back(this->current_state.getExpr());
			if (this->current_state.isNamespaceLike()) {
				auto namespace_expr = makeBox<IdentifierExpr>(
					query_ctx, this->current_state.getNamespaceLikeSymbol()
				);
				result_sequence.emplace_back(std::move(namespace_expr));
			}

			if (result_sequence.empty()) {
				query_ctx.log(
					dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
						chain_elements[0].unlock(query_ctx)->getSourcePosition(),
						"Chain expression is empty"
					)
				);
				return query::QError(errors::Failed());
			}

			if (result_sequence.size() == 1) return std::move(result_sequence[0]);

			// If we have multiple expressions, we need to create a chain expression
			auto chain_expr = makeBox<SequenceExpr>(query_ctx, std::move(result_sequence));
			return chain_expr;
		}
	};

	ExprConstructionResult fromChainExpr(
		query::Context& ctx, pst::AccessLocked<pst::expr::ChainExpr> expr
	) {
		ChainExprConstruction construction(ctx, expr.unlock(ctx));
		return construction.run();
	}
}
