/**
 * @file chain_expr.cpp
 * @author Wojciech Rzepliński
 */

#include "chain_expr.hpp"

#include "errors.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/expressions/function_calls/call_processing.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>
#include <token_parser_core/common_elements.hpp>

namespace compiler::helios::code {

	/**
	 * @brief This error message is used when there are both function symbols and non-function valid
	 * symbols found during the lookup (like function and class constructor with the same name).
	 */
	class CallInvalidCallablesError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_invalid_callables" };
		}

	public:
		class InvaidCallableReferenceDocs final: public dia_int::MessageBase {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "docs",
					     .family        = "expressions",
					     .name          = "invalid_callable_reference" };
			}

		public:
			InvaidCallableReferenceDocs(): MessageBase() {}
		};

		class CandidateNote final: public dia_int::MessageWithCodeFragmentAndCause {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "note",
					     .family        = "type_check",
					     .name          = "callable_candidate" };
			}

		public:
			CandidateNote(dia::SourcePosition source_position):
				  MessageWithCodeFragmentAndCause(source_position) {}
		};

		CallInvalidCallablesError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {
			addAttachedMessage(makeBox<InvaidCallableReferenceDocs>());
		}
	};

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
	class ChainExprConstruction final {
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
		 * The temporary buffor for the currently built value from left to current place of
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

		/**
		 * @brief Resolves a set of symbols which act as callees into a set of function symbols.
		 *
		 * The resolution follows the following steps:
		 * 1. If the set consists of only function symbols, return them as is.
		 * 2. If the set consists of multiple symbols, but one of them is not a function symbol,
		 *    log an error and return an empty set.
		 * 3. Otherwise, the set consists of a single non-function symbol. Then, look up
		 *    its call operators (like `operator()`, or constructors of a class).
		 *
		 * This implements
		 * https://docs.duckling.pl/duckling/writing_code/expressions/expression_types/call.html#callee-expression
		 *
		 * @param looked_up_callees The result of the lookup for the function being called.
		 * @return Candidates after resolution of functions vs call operators.
		 */
		[[nodiscard]]
		query::QResult<std::vector<SymID>> getCallableCandidates(
			const std::vector<SymID>& looked_up_callees
		) const {
			// If all candidates are functions, return them as is.
			if (std::ranges::all_of(looked_up_callees, [&](const SymID symbol) {
					return kind(symbol) == SymbolKind::Function
				        || kind(symbol) == SymbolKind::FunctionDeclaration;
				}))
				return looked_up_callees;

			// Check error condition and report error.
			if (looked_up_callees.size() > 1) {
				auto error = makeBox<CallInvalidCallablesError>(
					chain_elements.at(index).unlock(query_ctx)->getSourcePosition()
				);
				for (const auto& candidate: looked_up_callees) {
					error->addAttachedMessage(makeBox<CallInvalidCallablesError::CandidateNote>(
						stmt(query_ctx, candidate).value()->getSourcePosition()
					));
				}
				query_ctx.logInt(std::move(error));
				return query::Failed();
			}

			// We have a single non-function candidate. Perform lookup for its call operators.
			// @TODO: #982 #1532 Perform proper lookup in type for different cases.
			switch (auto symbol = looked_up_callees.front(); kind(symbol)) {
			case SymbolKind::Class: {
				// Retrieve constructors of the class.
				// @TODO: #1290 Handle auxiliary constructors.
				auto class_type = query_ctx.query<QueryTypeFromDefinition>({ symbol })
				                      ->valueOrThrow()
				                      .getType()
				                      .as<tsh::ClassAbstractType>();
				const auto& ctor
					= query_ctx.query<houtgen::QueryImplicitClassConstructor>({ class_type })
				          ->valueOrThrow();
				return std::vector{ ctor.declaration->original_symbol };
			}
			default:
				CORE_PANIC("Not implemented yet (", name(symbol), ")");
			}
			CORE_UNREACHABLE();
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
		) -> query::QResult<ChainState> {
			if (call_expr->getType() != lexer::Token::Round) {
				throw base::NotYetImplemented(base::strConcat(
					"HOUT call with invalid bracket type: ", char(call_expr->getType())
				));
			}
			const auto scope = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			const auto lookup_result
				= HInterface::ofScopeWithParents(scope).lookup(query_ctx, ident->getName().value);

			// @TODO: #1412 fix dealias
			const auto callees_q_result = getCallableCandidates(lookup_result->leaves);
			UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

			auto res = processFunctionCall(query_ctx, callees, call_expr);
			UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
			return ChainState::ofExpr(std::move(expr));
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * It is when we have keyword literal followed by a call expression, like "i64(42)".
		 * Currently used only for type casts.
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::KeywordLiteral> keyword, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState> {
			//  @TODO: #1530 This is a temporary mock implementation
			auto hout_expr_result = query_ctx.query<QueryHoutOfExpr>({ keyword });
			UNPACK_QRESULT_MOVE(base::Box<Expr> hout_expr =, hout_expr_result);

			if (auto literal_type_expr = dynamic_cast<LiteralTypeExpr*>(hout_expr.get())) {
				auto args = call_expr->getArgs().unlock(query_ctx);
				if (args->size() != 1) {
					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Type cast must have exactly one argument.", call_expr->getSourcePosition()
					));
					return query::Failed();
				}
				// Iterating over a single argument list, because the pst arguments
				// have only iterator accessor.
				for (auto&& arg: *args) {
					auto arg_expr_result = query_ctx.query<QueryHoutOfExpr>(
						arg.unlock(query_ctx)->getArg().unlock(query_ctx)->getExpr()
					);
					UNPACK_QRESULT_MOVE(base::Box<Expr> arg_expr =, arg_expr_result);

					auto cast_expr = makeBox<CastExpr>(
						query_ctx, std::move(arg_expr), literal_type_expr->value_type
					);
					return ChainState::ofExpr(std::move(cast_expr));
				}
			}

			query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				"Unsupported keyword literal in call expression.", call_expr->getSourcePosition()
			));
			return query::Failed();
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * Case when as a first element we have an identifier not followed by a call expression,
		 * like "foo.bar.c".
		 */
		auto processPSTExpr(pst::Access<pst::expr::IdentifierLiteral> ident)
			-> query::QResult<ChainState> {
			// Lookup global for const/variables/namespaces. Depending on the type of found
			// identifier it will return ChainContext with namespace or expr.
			auto        scope         = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			const auto& lookup_result = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
				ident->getName().position, query_ctx, ident->getName().value
			);
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);
			return processNamespaceOrValue(sym_list.back());
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * Case when as a first element in the chain
		 * is a more complicated expression like (NS1.NS2).a.b.c
		 */
		auto processPSTExpr(pst::Access<pst::ExprElement> pst_expr) -> query::QResult<ChainState> {
			auto expr = query_ctx.query<QueryHoutOfExpr>({ pst_expr });
			UNPACK_QRESULT_MOVE(base::Box<Expr> hout_expr =, expr);
			auto symbol = getIdentifierExprSymID(hout_expr.ref());
			if (symbol.has_value()) return processNamespaceOrValue(symbol.value());
			return ChainState::ofExpr(std::move(hout_expr));
		}

		/**
		 * Call in situations were we don't have "access expr" then "call expr" in a row,
		 * for example we have two call expr like a[i]() or b()()
		 */
		auto processPSTExpr(Box<Expr>, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState> {
			// @note this function is not run yet.

			// @note: previous mock-implementation of this function
			// was deleted in PR #1239. See it for reference.

			// @TODO: #982 improve type lookup and provide correct
			// candidates for processFunctionCall. write tests for this case, when it will be implemented

			auto expr_result = processFunctionCall(query_ctx, /* provide */ {}, call_expr);
			UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
			return ChainState::ofExpr(std::move(expr));
		}

		/**
		 * Call on the namespace, for example Namespace()
		 */
		auto processPSTExpr(SymID namespace_like_symbol, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState> {
			(void) namespace_like_symbol;
			query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				base::strConcat("Namespace is not callable"), call_expr->getSourcePosition()
			));
			return query::Failed();
		}

		/**
		 * Case when we have an access expression not followed by a call expression,
		 * for example "not_namespace.y", "x.y.c"
		 * and the current state is an expression (not a namespace e.x.).
		 * Should check if accessed field is a namespace or not.
		 */
		auto processPSTExpr(base::Box<Expr> current_expr, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<ChainState> {
			auto current_expr_type = current_expr->expression_type.getType();
			auto lookup_result     = HInterface::ofTypeInstance(current_expr_type)
			                         .lookup(query_ctx, expr_access->getName().value);

			const auto& looked_up_symbols_result = lookup_result->getAsSingle();
			// Note that if multiple symbols were found, it results in an error and enters
			// the following if statement. This is temporary, as symbol ambiguity should be
			// handled differently than through dynamic field access.
			UNPACK_QRESULT_MOVE(const auto& looked_up_symbols =, looked_up_symbols_result);

			variant_match(looked_up_symbols) {
				variant_case(SymbolList, result) {
					// @TODO: #1412 handle dealias expressions:
					auto sym = result.back();

					if (kind(sym) == SymbolKind::Field) {
						// Insert a deref if source of field access is not a direct type.
						if (current_expr->expression_type.getSymbolType().getRefKind()
						    != tsh::ReferenceKind::Direct) {
							current_expr = makeBox<DerefExpr>(query_ctx, std::move(current_expr));
						}
						auto node = makeBox<AccessExpr>(query_ctx, std::move(current_expr), sym);
						return ChainState::ofExpr(std::move(node));
					} else if (kind(sym) == SymbolKind::Namespace) {
						result_sequence.push_back(std::move(current_expr));
						return ChainState::ofNamespaceLike(sym);
					} else if (kind(sym) == SymbolKind::Method) {
						throw base::NotYetImplemented(
							"Handling of access to method without a call is not implemented yet"
						);
					}
					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						base::strConcat(
							"Unsupported symbol kind in type lookup for symbol: ",
							prettyDebugPrint(sym, query_ctx)
						),
						expr_access->getName().position
					));
					// @TODO: #1412 Support lookup of other kinds of symbols in classes.
					return query::Failed();
				}
				variant_case_novalue(errors::Ambiguity) {
					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Accessed value is ambiguous.", expr_access->getName().position
					));
					return query::Failed();
				}
				variant_case_novalue(errors::SymbolNotFound) {
					// @TODO: #1472 Handle dynamic field/method names, a.k.a. access operator
					// overloads. Ex.: obj.a fails to look up 'a', but it can still call
					// obj.selectDynamic("a"). See Scala's Dynamic:
					// https://www.scala-lang.org/api/current/scala/Dynamic.html

					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Accessed value not found.", expr_access->getName().position
					));
					return query::Failed();
				}

				variant_default { CORE_PANIC("Unexpected result type from lookup"); }
			}
			CORE_UNREACHABLE();
		}

		/**
		 * When we have an access expression not followed by a call expression,
		 * for example "NS.value" or "NS.value.y" amd the current state is a namespace.
		 */
		auto processPSTExpr(SymID namespace_like_symbol, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<ChainState> {
			const auto& lookup_result
				= HInterface::ofSymbol(namespace_like_symbol)
			          .lookupExpectUnique(
						  expr_access->getSourcePosition(), query_ctx, expr_access->getName().value
					  );
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);
			return processNamespaceOrValue(sym_list.back());
		}

		/**
		 * Case when we have an access expression followed by a call expression,
		 * and current state is an expression, for example "my_expr.foo()".
		   This may result in a method. Method
		 * parameter overload is possible.
		 */
		auto processPSTExpr(base::Box<Expr>, pst::Access<pst::expr::Access>, pst::Access<pst::expr::Call>)
			-> query::QResult<ChainState> {
			throw base::NotYetImplemented(
				"Helios chain expr: call on access expr not implemented yet"
			);
			return query::Failed();
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
		) -> query::QResult<ChainState> {
			auto lookup_result = HInterface::ofSymbol(namespace_like_symbol)
			                         .lookup(query_ctx, expr_access->getName().value);

			// @TODO: #1412 fix dealias
			auto callees_q_result = getCallableCandidates(lookup_result->leaves);
			UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

			auto expr_result = processFunctionCall(query_ctx, callees, call_expr);
			UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
			return ChainState::ofExpr(std::move(expr));
		}

		// ======================== MAIN PROCESSING FUNCTIONS HELPERS ========================


		/**
		 * Helper function of @p processPSTExpr that processes a value given the
		 * lookup result of the name.
		 */
		auto processNamespaceOrValue(const SymID& symbol) -> query::QResult<ChainState> {
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
				query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					base::strConcat(
						"Unsupported kind of the symbol `", name(symbol), "` in chain expression."
					),
					chain_elements.at(index).unlock(query_ctx)->getSourcePosition()
				));
				return query::Failed();
			}
		}

		// =============================== MAIN PROCESSING LOOP ===============================

		/**
		 * Perform a processing step on the current element of the chain.
		 * @warning It assumes that the current element exist.
		 */
		template<typename T>
		base::Optional<query::Failed> step() {
			auto current_element_value = currentElem().value().dynamicCast<T>().value();

			auto res = [&]() -> query::QResult<ChainState> {
				if (this->current_state.isExpr()) {
					auto expr = this->current_state.getExpr();
					return processPSTExpr(std::move(expr), current_element_value);
				} else if (this->current_state.isNamespaceLike()) {
					auto namespace_like_symbol = this->current_state.getNamespaceLikeSymbol();
					return processPSTExpr(namespace_like_symbol, current_element_value);
				}
				CORE_PANIC("Chain state is empty, but step() was called. This should not happen.");
			}();

			UNPACK_QRESULT_MOVE(this->current_state =, std::move(res));
			return {};
		}

		/**
		 * Perform a processing step on current element of the chain and next one.
		 * @warning It assumes that the current element and next one exist.
		 */
		template<typename T1, typename T2>
		base::Optional<query::Failed> step() {
			auto current_element_value = currentElem().value().dynamicCast<T1>().value();
			auto next_element_value    = nextElem().value().dynamicCast<T2>().value();
			auto res                   = [&]() -> query::QResult<ChainState> {
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
			UNPACK_QRESULT_MOVE(this->current_state =, std::move(res));
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T>
		base::Optional<query::Failed> firstStep() {
			CORE_ASSERT(
				this->current_state.isEmpty(), "Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T>().value();

			auto res = processPSTExpr(current_element_value);
			UNPACK_QRESULT_MOVE(this->current_state =, std::move(res));
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T1, typename T2>
		base::Optional<query::Failed> firstStep() {
			CORE_ASSERT(
				this->current_state.isEmpty(), "Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T1>().value();
			auto next_element_value    = nextElem().value().dynamicCast<T2>().value();
			auto res                   = processPSTExpr(current_element_value, next_element_value);
			UNPACK_QRESULT_MOVE(this->current_state =, std::move(res));
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
		query::QResult<base::Box<Expr>> run() {
			base::Optional<query::Failed> error{};

			this->index = 0;
			if (isCurrentElement<pst::expr::IdentifierLiteral>()
			    && isNextElement<pst::expr::Call>()) {
				error = firstStep<pst::expr::IdentifierLiteral, pst::expr::Call>();
				this->index += 2;
			} else if (isCurrentElement<pst::expr::KeywordLiteral>()
			           && isNextElement<pst::expr::Call>()) {
				error = firstStep<pst::expr::KeywordLiteral, pst::expr::Call>();
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
					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Expected access or call expression in chain expression",
						currentElem().value()->getSourcePosition()
					));
					return query::Failed();
				}
			}
			if (error.has_value()) return query::Failed();

			if (this->current_state.isExpr())
				result_sequence.push_back(this->current_state.getExpr());
			if (this->current_state.isNamespaceLike()) {
				auto namespace_expr = makeBox<IdentifierExpr>(
					query_ctx, this->current_state.getNamespaceLikeSymbol()
				);
				result_sequence.emplace_back(std::move(namespace_expr));
			}

			if (result_sequence.empty()) {
				query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Chain expression resulted in empty expression sequence.",
					chain_elements[0].unlock(query_ctx)->getSourcePosition()
				));
				return query::Failed();
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
