/**
 * @file chain_expr.cpp
 * @author Wojciech Rzepliński
 */

#include "chain_expr.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/expressions/function_calls/call_processing.hpp>
#include <helios_private/hout_creation/expressions/function_calls/square_call_processing.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

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
		class InvalidCallableReferenceDocs final: public dia_int::MessageBase {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "docs",
					     .family        = "expressions",
					     .name          = "invalid_callable_reference" };
			}

		public:
			InvalidCallableReferenceDocs(): MessageBase() {}
		};

		class CandidateNote final: public dia_int::MessageWithCodeFragmentAndCause {
			dia_int::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "note",
					     .family        = "type_check",
					     .name          = "callable_candidate" };
			}

		public:
			CandidateNote(dia_int::StablePosition source_position):
				  MessageWithCodeFragmentAndCause(source_position) {}
		};

		CallInvalidCallablesError(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {
			addAttachedMessage(makeBox<InvalidCallableReferenceDocs>());
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
			return namespace_like_symbol.value().first;
		}

		[[nodiscard]] auto getNamespaceLikePstOrigin() -> ElementOrigin {
			return namespace_like_symbol.value().second;
		}

		static ChainState ofExpr(base::Box<Expr> expr) { return { std::move(expr) }; }

		static ChainState ofNamespaceLike(SymID namespace_like_symbol, ElementOrigin origin) {
			return { namespace_like_symbol, origin };
		}

	private:
		ChainState(): expr(base::MBox<Expr>{}), namespace_like_symbol(std::nullopt) {}

		ChainState(base::Box<Expr> expr):
			  expr(std::move(expr)),
			  namespace_like_symbol(std::nullopt) {}

		ChainState(SymID namespace_like_symbol, ElementOrigin origin):
			  expr(base::MBox<Expr>{}),
			  namespace_like_symbol(std::make_pair(namespace_like_symbol, origin)) {}

		base::MBox<Expr>                                expr{};
		base::Optional<std::pair<SymID, ElementOrigin>> namespace_like_symbol{};

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
		 * The original PST chain expression that is being processed.
		 */
		pst::Access<pst::ExprElement> whole_chain_pst;

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
			// @TODO: #2135 handle ambiguity in class scopes

			// If all candidates are functions, return them as is.
			if (std::ranges::all_of(looked_up_callees, [&](const SymID symbol) {
					return kind(symbol) == SymbolKind::Function
				        || kind(symbol) == SymbolKind::FunctionDeclaration;
				}))
				return looked_up_callees;

			// If all candidates are methods, return them as is.
			if (std::ranges::all_of(looked_up_callees, [&](const SymID symbol) {
					return kind(symbol) == SymbolKind::Method;
				})) {
				return looked_up_callees;
			}

			// Check error condition and report error.
			if (looked_up_callees.size() > 1) {
				auto error = makeBox<CallInvalidCallablesError>(
					chain_elements.at(index).unlock(query_ctx)->getStablePosition()
				);
				for (const auto& candidate: looked_up_callees) {
					error->addAttachedMessage(makeBox<CallInvalidCallablesError::CandidateNote>(
						stmt(query_ctx, candidate).value()->getStablePosition()
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
					= query_ctx.query<defgen::QueryImplicitClassConstructor>({ class_type })
				          ->valueOrThrow();
				return std::vector{ ctor.declaration->original_symbol };
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat("Round Call '()' operator on symbol: ", name(symbol)),
					stmt(query_ctx, symbol).value()->getStablePosition()
				));
				return query::Failed();
			}
			}
			CORE_UNREACHABLE();
		}

		// =============================== MAIN PROCESSING FUNCTIONS ===============================

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain
		 * Case when we have a global identifier followed by a call expression,
		 * like "foo()" or array_like[i].
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::IdentifierLiteral> ident, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState> {
			const auto scope       = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			auto       h_interface = HInterface::ofScopeWithParents(scope);

			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				const auto lookup_qresult = h_interface.lookup(query_ctx, ident->getName().value);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);
				// @TODO: #1412 fix dealias
				const auto callees_q_result = getCallableCandidates(lookup_result->leaves);
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				auto res = processFunctionOrMethodNoSelfCall(query_ctx, callees, ident, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				const auto lookup_result = h_interface.lookupExpectUnique(
					ident->getStablePosition(), query_ctx, ident->getName().value
				);
				UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);

				auto base_state_res
					= processNamespaceOrValue(sym_list.back(), pstOrigin(ident), ident);
				UNPACK_QRESULT_MOVE(auto base_state =, base_state_res);


				if (base_state.isExpr()) {
					auto square_call_res
						= processSquareCall(query_ctx, base_state.getExpr(), call_expr);
					UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
					return ChainState::ofExpr(std::move(expr));
				}
				query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"This symbol cannot be indexed.", call_expr->getStablePosition()
				));
				return query::Failed();
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getStablePosition()
				));
				return query::Failed();
			}
			}
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * It is when we have keyword literal followed by a call expression. This currently includes:
		 * - `i64(42)` - used for explicit type casts.
		 * - `i64[42]` - used for static array type creation.
		 * - `List[i64]` - for dynamic array type creation.
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::KeywordLiteral> keyword, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState> {
			//  @TODO: #1530 This is a temporary mock implementation
			auto hout_expr_result = subExprFromPST(query_ctx, keyword);
			UNPACK_QRESULT_MOVE(auto hout_expr =, hout_expr_result);

			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				if (auto literal_type_expr = dynamic_cast<const LiteralTypeExpr*>(hout_expr.get())) {
					auto args = call_expr->getArgs().unlock(query_ctx);
					if (args->size() != 1) {
						query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
							"Type cast must have exactly one argument.",
							call_expr->getStablePosition()
						));
						return query::Failed();
					}

					auto arg_access      = (*args->begin()).unlock(query_ctx);
					auto arg_expr_result = subExprFromPST(
						query_ctx, arg_access->getArg().unlock(query_ctx)->getExpr()
					);
					UNPACK_QRESULT_MOVE(auto arg_expr =, arg_expr_result);

					auto cast_expr = makeBox<CastExpr>(
						query_ctx,
						multiplePstOriginOrdered({ keyword, call_expr }),
						std::move(arg_expr),
						literal_type_expr->value_type
					);
					return ChainState::ofExpr(std::move(cast_expr));
				}

				query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Unsupported keyword literal in call expression.", call_expr->getStablePosition()
				));
				break;
			}
			case lexer::Token::Square: {
				auto square_call_res
					= processSquareCall(query_ctx, std::move(hout_expr), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default:
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getStablePosition()
				));
				return query::Failed();
			}

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
				ident->getStablePosition(), query_ctx, ident->getName().value
			);
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);
			return processNamespaceOrValue(sym_list.back(), pstOrigin(ident), ident);
		}

		/**
		 * This function has no previous state argument so it is called as a first element in the
		 * chain.
		 * Case when as a first element in the chain
		 * is a more complicated expression like (NS1.NS2).a.b.c
		 */
		auto processPSTExpr(pst::Access<pst::ExprElement> pst_expr) -> query::QResult<ChainState> {
			auto expr_result = subExprFromPST(query_ctx, pst_expr);
			UNPACK_QRESULT_MOVE(auto expr =, expr_result);
			auto symbol = getIdentifierExprSymID(expr.ref());
			if (symbol.has_value())
				return processNamespaceOrValue(symbol.value(), pstOrigin(pst_expr), pst_expr);
			return ChainState::ofExpr(std::move(expr));
		}

		/**
		 * Call in situations were we don't have "access expr" then "call expr" in a row,
		 * for example we have two call expr like a[i]() or b()().
		 * @note: For now this function is called only in the `foo()[i]` case. (where foo is a
		 * function returning a static array).
		 */
		auto processPSTExpr(Box<Expr> current_expr, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState> {
			// @note: previous mock-implementation of this function
			// was deleted in PR #1239. See it for reference.
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				// @TODO: #982 improve type lookup and provide correct
				// candidates for processFunctionCall. Write tests for this case, when it will be
				// implemented.

				// This is only a temporary thing for error handling, note the incorrect callee PST
				// expression.
				auto expr_result
					= processFunctionCall(query_ctx, /* provide */ {}, call_expr, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				auto square_call_res
					= processSquareCall(query_ctx, std::move(current_expr), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getStablePosition()
				));
				return query::Failed();
			}
			}
		}

		/**
		 * Call on the namespace, for example Namespace()
		 */
		auto processPSTExpr(SymID namespace_like_symbol, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState> {
			(void) namespace_like_symbol;
			query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Namespace is not callable"), call_expr->getStablePosition()
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
			auto lookup_qresult    = HInterface::ofTypeInstance(current_expr_type)
			                          .lookup(query_ctx, expr_access->getName().value);
			UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);
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
							current_expr = makeBox<DerefExpr>(
								query_ctx,
								current_expr->origin.generatedFrom(),
								std::move(current_expr)
							);
						}
						auto node = makeBox<AccessExpr>(
							query_ctx,
							pstOriginOrdered(current_expr->origin, expr_access),
							std::move(current_expr),
							sym
						);
						return ChainState::ofExpr(std::move(node));
					} else if (kind(sym) == SymbolKind::Namespace) {
						result_sequence.push_back(std::move(current_expr));
						return ChainState::ofNamespaceLike(sym, pstOrigin(expr_access));
					} else if (kind(sym) == SymbolKind::Method) {
						query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
							"Handling of access to method without a call is not implemented yet"
							"argument at compile time",
							current_expr->origin.getStablePosition()
						));
						return query::Failed();
					}
					query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
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
					query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Accessed value is ambiguous.", expr_access->getName().position
					));
					return query::Failed();
				}
				variant_case_novalue(errors::SymbolNotFound) {
					// @TODO: #1472 Handle dynamic field/method names, a.k.a. access operator
					// overloads. Ex.: obj.a fails to look up 'a', but it can still call
					// obj.selectDynamic("a"). See Scala's Dynamic:
					// https://www.scala-lang.org/api/current/scala/Dynamic.html

					query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
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
						  expr_access->getStablePosition(), query_ctx, expr_access->getName().value
					  );
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);
			auto whole_expr_origin
				= pstOriginOrdered(current_state.getNamespaceLikePstOrigin(), expr_access);
			return processNamespaceOrValue(sym_list.back(), whole_expr_origin, expr_access);
		}

		/**
		 * Case when we have an access expression followed by a call expression,
		 * and current state is an expression.
		 * For example:
		 * - `my_expr.foo()` - this may result in a method. Method parameter overload is possible.
		 * - `class.array_field[ix]` - this is an index access the an array which is a class field.
		 */
		auto processPSTExpr(
			base::Box<Expr>                current_expr,
			pst::Access<pst::expr::Access> expr_access,
			pst::Access<pst::expr::Call>   call_expr
		) -> query::QResult<ChainState> {
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				auto current_expr_type = current_expr->expression_type.getType();
				auto lookup_qresult    = HInterface::ofTypeInstance(current_expr_type)
				                          .lookup(query_ctx, expr_access->getName().value);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				// @TODO: #1412 fix dealias
				const auto callees_q_result = getCallableCandidates(lookup_result->leaves);
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				auto self_expr_ref = makeBox<RefOfExpr>(
					query_ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
				);

				auto res = processMethodCall(
					query_ctx, callees, expr_access, call_expr, std::move(self_expr_ref)
				);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				auto access_res = processPSTExpr(std::move(current_expr), expr_access);
				UNPACK_QRESULT_MOVE(auto access_state =, access_res);

				if (access_state.isExpr()) {
					auto square_call_res
						= processSquareCall(query_ctx, access_state.getExpr(), call_expr);
					UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
					return ChainState::ofExpr(std::move(expr));
				}
				return query::Failed();
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getStablePosition()
				));
				return query::Failed();
			}
			}
		}

		/**
		 * Case when we have an access expression followed by a call expression,
		 * and current state is a namespace.
		 * For example:
		 * - `my_ns.foo()` - this may result in function overload.
		 * - `my_ns.array[ix]`
		 */
		auto processPSTExpr(
			SymID                          namespace_like_symbol,
			pst::Access<pst::expr::Access> expr_access,
			pst::Access<pst::expr::Call>   call_expr
		) -> query::QResult<ChainState> {
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				auto lookup_qresult = HInterface::ofSymbol(namespace_like_symbol)
				                          .lookup(query_ctx, expr_access->getName().value);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);
				// @TODO: #1412 fix dealias
				auto callees_q_result = getCallableCandidates(lookup_result->leaves);
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				auto expr_result
					= processFunctionOrMethodNoSelfCall(query_ctx, callees, expr_access, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				auto lookup_result = HInterface::ofSymbol(namespace_like_symbol)
				                         .lookupExpectUnique(
											 expr_access->getStablePosition(),
											 query_ctx,
											 expr_access->getName().value
										 );
				UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);

				auto whole_expr_origin
					= pstOriginOrdered(current_state.getNamespaceLikePstOrigin(), expr_access);
				auto state_res
					= processNamespaceOrValue(sym_list.back(), whole_expr_origin, expr_access);
				UNPACK_QRESULT_MOVE(auto access_state =, state_res);

				if (access_state.isExpr()) {
					auto square_call_res
						= processSquareCall(query_ctx, access_state.getExpr(), call_expr);
					UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
					return ChainState::ofExpr(std::move(expr));
				}


				query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"This symbol cannot be indexed", call_expr->getStablePosition()
				));
				return query::Failed();
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getStablePosition()
				));
				return query::Failed();
			}
			}
		}

		// ======================== MAIN PROCESSING FUNCTIONS HELPERS ========================


		/**
		 * Helper function of @p processPSTExpr that processes a value given the
		 * lookup result of the name.
		 * @param symbol The symbol found in the lookup.
		 * @param pst_elem The PST element for which the lookup was performed.
		 */
		auto processNamespaceOrValue(
			const SymID&                  symbol,
			ElementOrigin                 pst_element_origin,
			pst::Access<pst::LangElement> pst_elem
		) -> query::QResult<ChainState> {
			switch (kind(symbol)) {
			case SymbolKind::Namespace:
			case SymbolKind::Import: {
				return ChainState::ofNamespaceLike(symbol, pst_element_origin);
			}
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const: {
				auto expr = makeBox<IdentifierExpr>(query_ctx, pst_element_origin, symbol);
				return ChainState::ofExpr(std::move(expr));
			}
			case SymbolKind::Class: {
				auto type_qresult = query_ctx.query<QueryTypeFromDefinition>({ symbol });
				UNPACK_QRESULT_CREF(auto type_info =, type_qresult);

				auto expr
					= makeBox<LiteralTypeExpr>(query_ctx, pst_element_origin, type_info.getType());
				return ChainState::ofExpr(std::move(expr));
			}
			case SymbolKind::Field: {
				auto expr = processFieldNoSelf(query_ctx, symbol, pst_element_origin, pst_elem);
				UNPACK_QRESULT_MOVE(base::Box<Expr> field_expr =, expr);
				return ChainState::ofExpr(std::move(field_expr));
			}
			default:
				query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"Unsupported kind of the symbol `", name(symbol), "` in chain expression."
					),
					chain_elements.at(index).unlock(query_ctx)->getStablePosition()
				));
				return query::Failed();
			}
		}

		/**
		 * @brief When we have a field symbol found in lookup,
		 * that is not preceded by an access expression, for example "field"
		 * this functions finds the "self" argument
		 * and creates an access expression.
		 */
		auto processFieldNoSelf(
			query::Context&               ctx,
			const SymID&                  field_symbol,
			ElementOrigin                 pst_element_origin,
			pst::Access<pst::LangElement> pst_elem
		) -> query::QResult<base::Box<Expr>> {
			// @TODO: #2135 handle ambiguity in class scopes
			auto scope           = ctx.query<QueryPrimaryCodeScopeFor>({ pst_elem });
			auto sym_list_result = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
				pst_elem->getStablePosition(), ctx, base::StrID("self")
			);
			UNPACK_QRESULT_MOVE(const auto& sym_list =, sym_list_result);

			auto self_expr = makeBox<IdentifierExpr>(ctx, generatedOrigin(), sym_list.back());
			auto self_type = self_expr->expression_type.getSymbolType().getType();

			auto fields = self_type.getInterface(query_ctx)->getFieldsView();
			if (std::ranges::find(fields, field_symbol, &tsh::InterfaceElement::getSymbol)
			    == fields.end()) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"No such field found in the interface of prefix expression.",
					pst_elem->getStablePosition()
				));
				return query::Failed();
			}


			return makeBox<AccessExpr>(
				ctx,
				pst_element_origin,
				makeBox<DerefExpr>(ctx, generatedOrigin(), std::move(self_expr)),
				field_symbol
			);
		}

		/**
		 * @brief This function processes a function or method call when there is no "self" argument
		 * to find, for example "foo()". If the candidates are methods, it tries to find "self"
		 * argument and fails if it is not found. If the candidates are functions, it processes the
		 * call as a normal function call.
		 *
		 * @return query::QResult<base::Box<Expr>>
		 */
		auto processFunctionOrMethodNoSelfCall(
			query::Context&               ctx,
			const std::vector<SymID>&     candidates,
			pst::Access<pst::LangElement> callee_element,
			pst::Access<pst::expr::Call>  call_expr
		) -> query::QResult<base::Box<Expr>> {
			if (candidates.size() >= 1 && kind(candidates[0]) == SymbolKind::Method) {
				// @TODO: #2135 handle ambiguity in class scopes

				// Try to find "self" argument
				auto find_self_arg = [&] -> query::QResult<SymID> {
					auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ call_expr });

					auto lookup_qresult
						= HInterface::ofScopeWithParents(scope).lookup(ctx, base::StrID("self"));

					UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);
					UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result->getAsSingle());

					variant_match(sym_list) {
						variant_case(SymbolList, result) { return result.back(); }
						variant_case_novalue(errors::SymbolNotFound) {
							ctx.logInt(makeBox<dia_int::PlaceholderError>(
								"Method call without `self` argument.",
								call_expr->getStablePosition()
							));
							return query::Failed();
						}
						variant_case_novalue(errors::Ambiguity) {
							ctx.logInt(makeBox<dia_int::PlaceholderError>(
								"Multiple candidates for `self` argument found which should be "
								"impossible.",
								call_expr->getStablePosition()
							));
							return query::Failed();
						}
					}
					CORE_UNREACHABLE();
				};
				auto self_arg_result = find_self_arg();
				UNPACK_QRESULT(auto self_arg =, self_arg_result);
				auto self_expr = makeBox<IdentifierExpr>(ctx, generatedOrigin(), self_arg);

				// Filter candidates for which the self argument is different than the found one
				auto self_type = self_expr->expression_type;

				auto methods = self_type.getType().getInterface(ctx)->getMethodsView();

				std::vector<SymID> filtered_candidates;
				for (const auto& method: methods)
					if (std::ranges::find(candidates, method.getSymbol()) != candidates.end())
						filtered_candidates.push_back(method.getSymbol());

				return processMethodCall(
					ctx, filtered_candidates, callee_element, call_expr, std::move(self_expr)
				);
			}
			return processFunctionCall(ctx, candidates, callee_element, call_expr);
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
			  query_ctx(ctx),
			  whole_chain_pst(expr) {
			this->chain_elements.emplace_back(expr->getAtom());
			auto chain = expr->getChain();
			std::ranges::copy(chain, std::back_inserter(this->chain_elements));
		}

		ChainExprConstruction(query::Context& ctx, pst::Access<pst::expr::IdentifierLiteral> expr):
			  query_ctx(ctx),
			  whole_chain_pst(expr) {
			this->chain_elements.emplace_back(expr);
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
			} else if (isCurrentElement<pst::expr::IdentifierLiteral>()) {
				error = firstStep<pst::expr::IdentifierLiteral>();
				this->index++;
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
					query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Expected access or call expression in chain expression",
						currentElem().value()->getStablePosition()
					));
					return query::Failed();
				}
			}
			if (error.has_value()) return query::Failed();

			if (this->current_state.isExpr())
				result_sequence.push_back(this->current_state.getExpr());
			if (this->current_state.isNamespaceLike()) {
				auto namespace_expr = makeBox<IdentifierExpr>(
					query_ctx,
					this->current_state.getNamespaceLikePstOrigin(),
					this->current_state.getNamespaceLikeSymbol()
				);
				result_sequence.emplace_back(std::move(namespace_expr));
			}

			if (result_sequence.empty()) {
				query_ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Chain expression resulted in empty expression sequence.",
					chain_elements[0].unlock(query_ctx)->getStablePosition()
				));
				return query::Failed();
			}

			if (result_sequence.size() == 1) return std::move(result_sequence[0]);

			// If we have multiple expressions, we need to create a chain expression
			auto chain_expr = makeBox<SequenceExpr>(
				query_ctx, pstOrigin(whole_chain_pst), std::move(result_sequence)
			);
			return chain_expr;
		}
	};

	ExprConstructionResult fromChainExpr(
		query::Context& ctx, pst::AccessLocked<pst::expr::ChainExpr> expr
	) {
		ChainExprConstruction construction(ctx, expr.unlock(ctx));
		return construction.run();
	}

	query::QResult<Box<code::Expr>> fromIdentifierLiteral(
		query::Context& ctx, pst::AccessLocked<pst::expr::IdentifierLiteral> expr
	) {
		ChainExprConstruction construction(ctx, expr.unlock(ctx));
		return construction.run();
	}
}
