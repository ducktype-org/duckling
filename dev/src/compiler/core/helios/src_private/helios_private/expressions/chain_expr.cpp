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
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/function_calls/call_processing.hpp>
#include <helios_private/expressions/function_calls/square_call_processing.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/types.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include "diagnostic/source_position.hpp"
#include "query_framework/query_errors.hpp"
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::code {

	class ExprInterface final: public CustomInterfaceABC {
		query::QResult<SymID> getExprSymID(query::Context& ctx, CRef<Expr> expr) const {
			if (auto identifier_expr = dynamic_cast<const IdentifierExpr*>(expr.get()))
				return identifier_expr->symbol;

			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"Getting symbol ID from non-trivial expressions is not implemented yet",
				expr->origin.getSourcePosition()
			));
			return query::Failed();
		}

		CRef<query::QResult<LookupResult>> lookup(
			query::Context& ctx, base::StrID name, AdditionalLookupParameters params
		) override {
			auto type = expr->expression_type.getType();
			switch (type.getKind()) {
			case tsh::Kind::Namespace:
			case tsh::Kind::Module:
			case tsh::Kind::Import: {
				thread_local query::QResult<LookupResult> failed         = query::Failed();
				auto                                      sym_id_qresult = getExprSymID(ctx, expr);
				if (sym_id_qresult.hasFailed())
					return &failed;
				else {
					auto sym_id = sym_id_qresult.valueOrThrow();
					return HInterface::ofSymbol(sym_id).lookup(ctx, name, params);
				}
			}
			case tsh::Kind::Meta:
				return HInterface::ofTypeMeta(type).lookup(ctx, name, params);
			default:
				// Note, there are types that maybe shouldn't be looked up in, but we will still
				// record an error there.
				return HInterface::ofTypeInstance(type).lookup(ctx, name, params);
			}
		}

	public:
		ExprInterface(CRef<Expr> expr): expr(expr) {}

		static HInterface ofExpr(CRef<Expr> expr) {
			return HInterface::ofCustom(makeBox<ExprInterface>(expr));
		}

		virtual ~ExprInterface() = default;

	private:
		CRef<Expr> expr;
	};
}

namespace compiler::helios::code {
	/**
	 * @brief This error message is used when there are both function symbols and non-function
	 * valid symbols found during the lookup (like function and class constructor with the same
	 * name).
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
			CandidateNote(dia::SourcePosition source_position):
				  MessageWithCodeFragmentAndCause(source_position) {}
		};

		CallInvalidCallablesError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {
			addAttachedMessage(makeBox<InvalidCallableReferenceDocs>());
		}
	};

	/**
	 * Constructs an expression from a PST chain expression, for example from
	 * "a.b.c().d[20].W". It processes the chain from left to right, building the expression
	 * step by step. The state of the building process is stored in value of class @p
	 * ChainState.
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
	 * @note It can also return a identifier expression with a namespace, since "(NS.NS2).a" is
	 * a thing in our compiler (namespace is a valid type that can be for example passed to a
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
		MBox<Expr> current_expr;

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
			query::Context& ctx,
			const std::vector<SymID>& looked_up_callees,
			dia::SourcePosition call_position
		) const {
			// @TODO: #2135 handle ambiguity in class scopes

			if (looked_up_callees.empty()) {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>("No callables found for call.", call_position));
				return query::Failed();
			}

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
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat("Round Call '()' operator on symbol: ", name(symbol)),
					stmt(query_ctx, symbol).value()->getSourcePosition()
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
		) -> query::QResult<Box<Expr>> {
			const auto scope       = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			auto       h_interface = HInterface::ofScopeWithParents(scope);

			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				const auto lookup_qresult = h_interface.lookup(query_ctx, ident->getName().value);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);
				// @TODO: #1412 fix dealias
				const auto callees_q_result = getCallableCandidates(query_ctx, lookup_result->leaves, call_expr->getSourcePosition());
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				auto res = processFunctionOrMethodNoSelfCall(query_ctx, callees, ident, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
				return expr;
			}
			case lexer::Token::Square: {
				const auto lookup_result = h_interface.lookupExpectUnique(
					ident->getSourcePosition(), query_ctx, ident->getName().value
				);
				UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);

				auto base_expr_res
					= processNamespaceOrValue(sym_list.back(), pstOrigin(ident), ident);
				UNPACK_QRESULT_MOVE(auto base_expr =, base_expr_res);


				auto square_call_res
					= processSquareCall(query_ctx, std::move(base_expr), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return expr;
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getSourcePosition()
				));
				return query::Failed();
			}
			}
		}

		/**
		 * This function has no previous state argument so it is called as a first element in
		 * the chain. It is when we have keyword literal followed by a call expression. This
		 * currently includes:
		 * - `i64(42)` - used for explicit type casts.
		 * - `i64[42]` - used for static array type creation.
		 * - `List[i64]` - for dynamic array type creation.
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::KeywordLiteral> keyword, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<Box<Expr>> {
			//  @TODO: #1530 This is a temporary mock implementation
			auto hout_expr_result = query_ctx.query<QueryHoutOfExpr>({ keyword });
			UNPACK_QRESULT_CREF_TO_BOX(CRef<Expr> hout_expr =, hout_expr_result);

			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				if (auto literal_type_expr = dynamic_cast<const LiteralTypeExpr*>(hout_expr.get())) {
					auto args = call_expr->getArgs().unlock(query_ctx);
					if (args->size() != 1) {
						query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
							"Type cast must have exactly one argument.",
							call_expr->getSourcePosition()
						));
						return query::Failed();
					}

					auto arg_access      = (*args->begin()).unlock(query_ctx);
					auto arg_expr_result = query_ctx.query<QueryHoutOfExpr>(
						arg_access->getArg().unlock(query_ctx)->getExpr()
					);
					UNPACK_QRESULT_CREF_TO_BOX(CRef<Expr> arg_expr =, arg_expr_result);

					auto cast_expr = makeBox<CastExpr>(
						query_ctx,
						multiplePstOrigin({ keyword, call_expr }),
						arg_expr->clone(),
						literal_type_expr->value_type
					);
					return cast_expr;
				}

				query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Unsupported keyword literal in call expression.", call_expr->getSourcePosition()
				));
				break;
			}
			case lexer::Token::Square: {
				auto square_call_res = processSquareCall(query_ctx, hout_expr->clone(), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return expr;
			}
			default:
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getSourcePosition()
				));
				return query::Failed();
			}

			return query::Failed();
		}

		/**
		 * This function has no previous state argument so it is called as a first element in
		 * the chain. Case when as a first element we have an identifier not followed by a call
		 * expression, like "foo.bar.c".
		 */
		auto processPSTExpr(pst::Access<pst::expr::IdentifierLiteral> ident)
			-> query::QResult<Box<Expr>> {
			// Lookup global for const/variables/namespaces. Depending on the type of found
			// identifier it will return ChainContext with namespace or expr.
			auto        scope         = query_ctx.query<QueryPrimaryCodeScopeFor>({ ident });
			const auto& lookup_result = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
				ident->getName().position, query_ctx, ident->getName().value
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
		auto processPSTExpr(pst::Access<pst::ExprElement> pst_expr) -> query::QResult<Box<Expr>> {
			auto expr = query_ctx.query<QueryHoutOfExpr>({ pst_expr });
			UNPACK_QRESULT_CREF_TO_BOX(CRef<Expr> hout_expr =, expr);
			auto symbol = getIdentifierExprSymID(hout_expr);
			if (symbol.has_value())
				return processNamespaceOrValue(symbol.value(), pstOrigin(pst_expr), pst_expr);
			return hout_expr->clone();
		}

		/**
		 * Call in situations were we don't have "access expr" then "call expr" in a row,
		 * for example we have two call expr like a[i]() or b()().
		 * @note: For now this function is called only in the `foo()[i]` case. (where foo is a
		 * function returning a static array).
		 */
		auto processPSTExpr(Box<Expr> current_expr, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<Box<Expr>> {
			// @note: previous mock-implementation of this function
			// was deleted in PR #1239. See it for reference.
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				// @TODO: #982 improve type lookup and provide correct
				// candidates for processFunctionCall. Write tests for this case, when it will
				// be implemented.

				// This is only a temporary thing for error handling, note the incorrect callee
				// PST expression.
				auto expr_result
					= processFunctionCall(query_ctx, /* provide */ {}, call_expr, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
				return expr;
			}
			case lexer::Token::Square: {
				auto square_call_res
					= processSquareCall(query_ctx, std::move(current_expr), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return expr;
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getSourcePosition()
				));
				return query::Failed();
			}
			}
		}

		/**
		 * Case when we have an access expression not followed by a call expression,
		 * for example "not_namespace.y", "x.y.c"
		 * and the current state is an expression (not a namespace e.x.).
		 * Should check if accessed field is a namespace or not.
		 */
		auto processPSTExpr(base::Box<Expr> current_expr, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<Box<Expr>> {
			auto sym_list_result
				= ExprInterface::ofExpr(current_expr.ref())
			          .lookupExpectUnique(
						  expr_access->getSourcePosition(), query_ctx, expr_access->getName().value
					  );

			UNPACK_QRESULT_MOVE(const auto& sym_list =, sym_list_result);
			auto sym = sym_list.back();

			if (kind(sym) == SymbolKind::Field) {
				// Insert a deref if source of field access is not a direct type.
				if (current_expr->expression_type.getSymbolType().getRefKind()
				    != tsh::ReferenceKind::Direct) {
					current_expr = makeBox<DerefExpr>(
						query_ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
					);
				}
				auto node = makeBox<AccessExpr>(
					query_ctx,
					pstOrigin(current_expr->origin, expr_access),
					std::move(current_expr),
					sym
				);
				return node;
			} else {
				auto origin = pstOrigin(current_expr->origin, expr_access);
				return processNamespaceOrValue(sym, origin, expr_access);
			}
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
		) -> query::QResult<Box<Expr>> {
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				auto lookup_qresult = ExprInterface::ofExpr(current_expr.ref())
				                          .lookup(query_ctx, expr_access->getName().value);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				// @TODO: #1412 fix dealias
				const auto callees_q_result = getCallableCandidates(query_ctx, lookup_result->leaves, call_expr->getSourcePosition());
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				// if (callees.back())
				if (kind(callees.back()) == SymbolKind::Method) {
					auto self_expr_ref = makeBox<RefOfExpr>(
						query_ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
					);

					auto res = processMethodCall(
						query_ctx, callees, expr_access, call_expr, std::move(self_expr_ref)
					);
					UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);

					return expr;
				} else {
					auto res = processFunctionCall(query_ctx, callees, expr_access, call_expr);
					UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
					return expr;
				}
			}
			case lexer::Token::Square: {
				auto access_res = processPSTExpr(std::move(current_expr), expr_access);
				UNPACK_QRESULT_MOVE(auto base_expr =, access_res);

				auto square_call_res
					= processSquareCall(query_ctx, std::move(base_expr), call_expr);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return expr;
			}
			default: {
				query_ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"HOUT call with unsupported bracket type: ", char(call_expr->getType())
					),
					call_expr->getSourcePosition()
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
		) -> query::QResult<Box<Expr>> {
			switch (kind(symbol)) {
			case SymbolKind::Namespace:
			case SymbolKind::Import:
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const:
			case SymbolKind::Class: {
				return makeBox<IdentifierExpr>(query_ctx, pst_element_origin, symbol);
			}
			case SymbolKind::Field: {
				auto expr = processFieldNoSelf(query_ctx, symbol, pst_element_origin, pst_elem);
				UNPACK_QRESULT_MOVE(base::Box<Expr> field_expr =, expr);
				return field_expr;
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
				pst_elem->getSourcePosition(), ctx, base::StrID("self")
			);
			UNPACK_QRESULT_MOVE(const auto& sym_list =, sym_list_result);

			auto self_expr = makeBox<IdentifierExpr>(ctx, generatedOrigin(), sym_list.back());
			auto self_type = self_expr->expression_type.getSymbolType().getType();

			auto fields = self_type.getInterface(ctx)->getFieldsView();
			if (std::ranges::find(fields, field_symbol, &tsh::InterfaceElement::getSymbol)
			    == fields.end()) {
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"No such field found in the interface of prefix expression.",
					pst_elem->getSourcePosition()
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
		 * @brief This function processes a function or method call when there is no "self"
		 * argument to find, for example "foo()". If the candidates are methods, it tries to
		 * find "self" argument and fails if it is not found. If the candidates are functions,
		 * it processes the call as a normal function call.
		 *
		 * @return query::QResult<base::Box<Expr>>
		 */
		auto processFunctionOrMethodNoSelfCall(
			query::Context&               ctx,
			const std::vector<SymID>&     candidates,
			pst::Access<pst::LangElement> callee_element,
			pst::Access<pst::expr::Call>  call_expr
		) -> query::QResult<base::Box<CallExpr>> {
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
							ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
								"Method call without `self` argument.",
								call_expr->getSourcePosition()
							));
							return query::Failed();
						}
						variant_case_novalue(errors::Ambiguity) {
							ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
								"Multiple candidates for `self` argument found which should be "
								"impossible.",
								call_expr->getSourcePosition()
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
			UNPACK_QRESULT_MOVE(
				this->current_expr =,
				processPSTExpr(
					std::move(this->current_expr).toOptBox().value(), current_element_value
				)
			);
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
			UNPACK_QRESULT_MOVE(
				this->current_expr =,
				processPSTExpr(
					std::move(this->current_expr).toOptBox().value(),
					current_element_value,
					next_element_value
				)
			);
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T>
		base::Optional<query::Failed> firstStep() {
			CORE_ASSERT(
				this->current_expr.toOpt().empty(),
				"Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T>().value();

			UNPACK_QRESULT_MOVE(this->current_expr =, processPSTExpr(current_element_value));
			return {};
		}

		/**
		 * Same as above, but it performs a step on the first element of the chain, where
		 * chain state is empty.
		 */
		template<typename T1, typename T2>
		base::Optional<query::Failed> firstStep() {
			CORE_ASSERT(
				this->current_expr.toOpt().empty(),
				"Chain state should be empty when firstStep is called"
			);
			auto current_element_value = currentElem().value().dynamicCast<T1>().value();
			auto next_element_value    = nextElem().value().dynamicCast<T2>().value();
			UNPACK_QRESULT_MOVE(
				this->current_expr =, processPSTExpr(current_element_value, next_element_value)
			);
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
					query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
						"Expected access or call expression in chain expression",
						currentElem().value()->getSourcePosition()
					));
					return query::Failed();
				}
			}
			if (error.has_value()) return query::Failed();

			if_opt_some(std::move(this->current_expr).toOptBox(), expr)
				result_sequence.push_back(std::move(expr));

			if (result_sequence.empty()) {
				query_ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Chain expression resulted in empty expression sequence.",
					chain_elements[0].unlock(query_ctx)->getSourcePosition()
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
