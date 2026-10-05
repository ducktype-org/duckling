// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file chain_expr.cpp
 * @author Wojciech Rzepliński
 */

#include "chain_expr.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios/utils/get_expr_symid.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/function_calls/call_processing.hpp>
#include <helios_private/hout_creation/expressions/function_calls/square_call_processing.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/templates/templates.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

#include <ranges>

namespace compiler::helios::code {

	namespace {
		/**
		 * @brief Helper to get the interface element of a symbol that is a type member.
		 */
		CRef<tsh::InterfaceElement> getInterfaceElementOfMemberSym(query::Context& ctx, SymID sym) {
			return typeMemberOwner(sym).getInterface(ctx)->getElementBySym(sym).value();
		}
	}

	/**
	 * This is temporary helper used before #3095.
	 * It is written in a way that it can be used with minimal boilerplate with the current chain
	 * chain expression processing code (as both chain expression processing and this function are
	 * soon to be refactored)
	 *
	 * @TODO: #3095 remove it, move the relevant code to the handling of the new :{} PST node.
	 *
	 * @note Usage of this function is added only in few places for now, after #3095 the number of
	 * required places to handle templates here will be much lower either way.
	 *
	 * @return QResult<base::Optional<SymID>> - the resulting symbol id of the baked template, or
	 * empty if the template did not happen (with standard QResult semantics on top of it).
	 */
	template<typename T>
	query::QResult<base::Optional<SymID>> transformTemplateBake(
		query::Context& query_ctx,
		SymID           template_sym_id,
		pst::Access<T>  element_with_template_specifier
	) requires requires(T t) { t.getTemplateSpecifier(); } {
		if (not element_with_template_specifier->getTemplateSpecifier().has_value())
			return base::Optional<SymID>{};  // no template specifier, no bake

		if (kind(template_sym_id) != SymbolKind::Template) {
			query_ctx.logInt(makeBox<dia::PlaceholderError>(
				"template bake called on non-template symbol",
				element_with_template_specifier->getStablePosition()
			));
			return query::Failed();
		}

		auto template_specifier = element_with_template_specifier->getTemplateSpecifier().value();
		auto template_specifier_dc
			= template_specifier.template dynamicCast<pst::expr::TemplateSpecifier>().unlock(
				query_ctx
			);

		templates::TemplateBakeKey key{
			.template_sym_id    = template_sym_id,
			.template_arguments = {},
		};

		auto template_signature_qr
			= templates::getTemplateDeclarationSignature(query_ctx, template_sym_id);
		if (template_signature_qr.hasFailed()) return query::Failed();
		const auto& template_signature = template_signature_qr.valueOrPanic();

		auto argument_list = template_specifier_dc->getArgumentList().unlock(query_ctx);

		if (template_signature.parameters.size() != argument_list->size()) {
			query_ctx.logInt(makeBox<dia::PlaceholderError>(
				"argument count does not match template parameter count",
				element_with_template_specifier->getStablePosition()
			));
			return query::Failed();
		}

		for (const auto& [arg, parameter]:
		     std::views::zip(*argument_list, template_signature.parameters)) {
			auto arg_unlocked    = arg.unlock(query_ctx);
			auto arg_expr_result = subExprFromPST(query_ctx, arg_unlocked->getExpr());

			if (arg_expr_result.hasFailed()) return query::Failed();
			auto arg_expr = std::move(arg_expr_result).valueOrPanic();
			UNPACK_QRESULT_MOVE(
				auto coerced_arg_expr =,
				coerceFromBox(
					query_ctx, std::move(arg_expr), parameter.type, arg_unlocked->getStablePosition()
				)
			);

			auto ctv = query_ctx.query<QueryEvaluateHOUTExpression>({ coerced_arg_expr.ref() });

			if (ctv.hasFailed()) return query::Failed();
			auto ctv_value = std::move(ctv).valueOrPanic();

			key.template_arguments.emplace_back(std::move(ctv_value));
		}

		auto resulting_symbol = query_ctx.query<helios::templates::QueryBakeTemplateSymID>({ key });

		if (resulting_symbol.hasFailed()) return query::Failed();
		return resulting_symbol.valueOrPanic();
	}

	/**
	 * This is temporary helper used before #3095 and before #3112
	 *
	 * @TODO: #3095 remove or adjust it, move the relevant code to the handling of the new :{} PST
	 * node.
	 */
	template<typename T>
	query::QResult<base::Optional<SymID>> transformTemplateBakeLookupResult(
		query::Context&    query_ctx,
		CRef<LookupResult> lookup_result,
		pst::Access<T>     element_with_template_specifier
	) requires requires(T t) { t.getTemplateSpecifier(); } {
		if (not element_with_template_specifier->getTemplateSpecifier().has_value())
			return base::Optional<SymID>{};  // no template specifier, no bake

		auto as_single = lookup_result->getAsSingle();

		if (as_single.hasFailed()) return query::Failed();

		variant_match(as_single.valueOrPanic()) {
			variant_case(SymbolList, symbol_list) {
				CORE_ASSERT(
					not symbol_list.empty(), "Invalid state: empty symbol list in lookup result"
				);

				// @TODO: #1412 fix dealias, this discards all aliases and takes the last symbol in
				// the list
				auto template_sym_id = symbol_list.list.back();
				return transformTemplateBake(
					query_ctx, template_sym_id, element_with_template_specifier
				);
			}

			variant_case(errors::Ambiguity, _) {
				query_ctx.logInt(makeBox<dia::PlaceholderError>(
					"template bake called on ambiguous lookup result",
					element_with_template_specifier->getStablePosition()
				));
				return query::Failed();
			}

			variant_case(errors::SymbolNotFound, _) {
				query_ctx.logInt(makeBox<dia::PlaceholderError>(
					"Symbol not found in lookup",
					element_with_template_specifier->getStablePosition()
				));
				return query::Failed();
			}

			variant_case(errors::Inaccessible, _) {
				query_ctx.logInt(makeBox<dia::PlaceholderError>(
					"Symbol found in lookup is not visible from here",
					element_with_template_specifier->getStablePosition()
				));
				return query::Failed();
			}

			variant_default { CORE_PANIC("Invalid state: unexpected variant in lookup result"); }
		}

		CORE_UNREACHABLE();
	}

	/**
	 * @brief This error message is used when there are both function symbols and non-function valid
	 * symbols found during the lookup (like function and class constructor with the same name).
	 */
	class CallInvalidCallablesError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_invalid_callables" };
		}

	public:
		class InvalidCallableReferenceDocs final: public dia::MessageBase {
			dia::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "docs",
					     .family        = "expressions",
					     .name          = "invalid_callable_reference" };
			}

		public:
			InvalidCallableReferenceDocs(): MessageBase() {}
		};

		class CandidateNote final: public dia::MessageWithCodeFragmentAndCause {
			dia::Metadata getMetadata() const final {
				return { .template_type = "message",
					     .type          = "note",
					     .family        = "type_check",
					     .name          = "callable_candidate" };
			}

		public:
			CandidateNote(dia::StablePosition source_position):
				  MessageWithCodeFragmentAndCause(source_position) {}
		};

		CallInvalidCallablesError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {
			addAttachedMessage(makeBox<InvalidCallableReferenceDocs>());
		}
	};

	/**
	 * @brief A type written in the chain, as in the `T` of `T.foo` or of `let t = T;`.
	 *
	 * It is kept as a type, and not as a `LiteralTypeExpr`, because what follows in the chain
	 * decides what it becomes: an access on it is a lookup in the type, while a type that ends
	 * the chain is a value of the meta type.
	 */
	struct TypeInChain final {
		tsh::AbstractType type;
		ElementOrigin     origin;
	};

	/**
	 * State for the building of the chain expression in HOUT.
	 * It mainly stores three distinct states:
	 * - namespace-like state, for example after processing "foo().Namespace"
	 * - type state, for example after processing "foo().MyClass"
	 * - expression state, for example after processing "foo().bar[20]"
	 *
	 * Its stores the state of chain expression creation that does not include
	 * all previously cut-off expressions.
	 */
	struct ChainState final {
		[[nodiscard]] bool isNamespaceLike() const { return namespace_like_symbol.has_value(); }

		[[nodiscard]] bool isType() const { return type.has_value(); }

		[[nodiscard]] bool isExpr() const { return expr.toOpt().has_value(); }

		[[nodiscard]] bool isEmpty() const {
			return not isNamespaceLike() and not isType() and not isExpr();
		}

		[[nodiscard]] auto getExpr() -> base::Box<Expr> {
			return std::move(expr).toOptBox().value();
		}

		[[nodiscard]] auto getNamespaceLikeSymbol() -> SymID {
			return namespace_like_symbol.value().first;
		}

		[[nodiscard]] auto getNamespaceLikePstOrigin() -> ElementOrigin {
			return namespace_like_symbol.value().second;
		}

		[[nodiscard]] auto getType() -> TypeInChain { return type.value(); }

		static ChainState ofExpr(base::Box<Expr> expr) { return { std::move(expr) }; }

		static ChainState ofNamespaceLike(SymID namespace_like_symbol, ElementOrigin origin) {
			return { namespace_like_symbol, origin };
		}

		static ChainState ofType(tsh::AbstractType type, ElementOrigin origin) {
			return ChainState{ { .type = type, .origin = origin } };
		}

		/**
		 * @brief The value a chain state holds, which for a type state is the type literal of the
		 * type it carries. Empty for states that hold no value, like a namespace.
		 */
		auto asExpr(query::Context& ctx) -> base::Box<Expr> {
			if (isExpr()) return getExpr();
			if (isType()) {
				auto type_in_chain = getType();
				return makeBox<LiteralTypeExpr>(ctx, type_in_chain.origin, type_in_chain.type);
			}
			if (isNamespaceLike()) {
				auto sym    = getNamespaceLikeSymbol();
				auto origin = getNamespaceLikePstOrigin();
				return makeBox<IdentifierExpr>(ctx, origin, sym);
			}
			CORE_PANIC("Invalid ChainState state.");
		}

	private:
		ChainState(): expr(base::MBox<Expr>{}), namespace_like_symbol(std::nullopt) {}

		ChainState(base::Box<Expr> expr):
			  expr(std::move(expr)),
			  namespace_like_symbol(std::nullopt) {}

		ChainState(SymID namespace_like_symbol, ElementOrigin origin):
			  expr(base::MBox<Expr>{}),
			  namespace_like_symbol(std::make_pair(namespace_like_symbol, origin)) {}

		explicit ChainState(TypeInChain type): expr(base::MBox<Expr>{}), type(type) {}

		base::MBox<Expr>                                expr{};
		base::Optional<std::pair<SymID, ElementOrigin>> namespace_like_symbol{};
		base::Optional<TypeInChain>                     type{};

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
			if (looked_up_callees.empty()) return std::vector<SymID>{};

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
				// Make sure that all of the found methods are either static methods or instance methods.
				auto type      = typeMemberOwner(looked_up_callees.at(0));
				auto interface = type.getInterface(query_ctx);
				auto elements  = looked_up_callees | std::views::transform([&](SymID symbol) {
                                    return interface->getElementBySym(symbol).value();
                                })
				              | std::ranges::to<std::vector<CRef<tsh::InterfaceElement>>>();

				if (std::ranges::all_of(looked_up_callees, [&](const SymID symbol) {
						return interface->getElementBySym(symbol).value()->isStaticMethod();
					}))
					return looked_up_callees;
				else if (std::ranges::all_of(looked_up_callees, [&](const SymID symbol) {
							 return interface->getElementBySym(symbol).value()->isMethod();
						 }))
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
				auto        class_type = query_ctx.query<tsh::QueryClassType>({ symbol });
				const auto& ctor
					= query_ctx.query<defgen::QueryImplicitClassConstructor>({ class_type })
				          ->valueOrThrow();
				return std::vector{ ctor.declaration->original_symbol };
			}
			default: {
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat("Round Call '()' operator on symbol: ", name(symbol)),
					stmt(query_ctx, symbol).value()->getStablePosition()
				));
				return query::Failed();
			}
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Bakes the callee if it carries a template specifier, then resolves the result
		 * into callable candidates.
		 *
		 * @param lookup_result The result of the lookup for the callee.
		 * @param element_with_template_specifier The PST element naming the callee.
		 * @return Candidates after baking and resolution of functions vs call operators.
		 *
		 * @TODO: #3095 fold into the handling of the new :{} PST node.
		 */
		template<typename T>
		[[nodiscard]]
		query::QResult<std::vector<SymID>> getCallableCandidatesWithBake(
			CRef<LookupResult> lookup_result, pst::Access<T> element_with_template_specifier
		) const {
			UNPACK_QRESULT(
				auto maybe_bake =,
				transformTemplateBakeLookupResult(
					query_ctx, lookup_result, element_with_template_specifier
				)
			);

			// @TODO: #1412 fix dealias
			if (maybe_bake.has_value())
				return getCallableCandidates(std::vector{ maybe_bake.value() });
			return getCallableCandidates(lookup_result->leaves);
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
				const auto lookup_qresult
					= h_interface.lookup(query_ctx, ident->getName().unlock(query_ctx)->unwrap());
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				UNPACK_QRESULT(
					const auto& callees =, getCallableCandidatesWithBake(lookup_result, ident)
				);

				auto res = processFunctionOrMethodNoSelfCall(query_ctx, callees, ident, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, res);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				const auto lookup_result = h_interface.lookupExpectUnique(
					ident->getStablePosition(),
					query_ctx,
					ident->getName().unlock(query_ctx)->unwrap()
				);
				UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);

				auto base_state_res
					= processNamespaceOrValue(sym_list.back(), pstOrigin(ident), ident);
				UNPACK_QRESULT_MOVE(auto base_state =, base_state_res);


				auto square_call_res = processSquareCall(
					query_ctx, std::move(base_state).asExpr(query_ctx), call_expr
				);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default: {
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
		 * - `i64[42]` - used for static array type creation.
		 *
		 * Note that `i64(42)` is no longer a type cast - the `as` operator (`42 as i64`) is the
		 * only supported explicit conversion syntax.
		 */
		auto processPSTExpr(
			pst::Access<pst::expr::KeywordLiteral> keyword, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState> {
			//  @TODO: #1530 This is a temporary mock implementation
			auto hout_expr_result = subExprFromPST(query_ctx, keyword);
			UNPACK_QRESULT_MOVE(auto hout_expr =, hout_expr_result);

			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				if (dynamic_cast<const LiteralTypeExpr*>(hout_expr.get()) != nullptr) {
					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						"A type cannot be called. Explicit conversions use the `as` operator.",
						call_expr->getStablePosition(),
						"Write `value as Type` instead of `Type(value)`."
					));
					return query::Failed();
				}

				query_ctx.logInt(makeBox<dia::PlaceholderError>(
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
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
				ident->getStablePosition(), query_ctx, ident->getName().unlock(query_ctx)->unwrap()
			);
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);
			auto mock_symbol = sym_list.back();

			UNPACK_QRESULT(
				auto maybe_template_bake =, transformTemplateBake(query_ctx, mock_symbol, ident)
			);

			if_opt_some(maybe_template_bake, template_bake) {
				return processNamespaceOrValue(template_bake, pstOrigin(ident), ident);
			}

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
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
		auto processPSTExpr(
			[[maybe_unused]] SymID namespace_like_symbol, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ChainState> {
			query_ctx.logInt(makeBox<dia::PlaceholderError>(
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
			auto lookup_qresult
				= HInterface::ofTypeInstance(current_expr_type)
			          .lookup(
						  query_ctx,
						  expr_access->getName().unlock(query_ctx)->unwrap(),
						  { .accessing_scope
			                = query_ctx.query<QueryPrimaryCodeScopeFor>({ expr_access }) }
					  );
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

					if (isStaticField(query_ctx, sym)) {
						auto node = makeBox<IdentifierExpr>(
							query_ctx, pstOriginOrdered(current_expr->origin, expr_access), sym
						);
						return ChainState::ofExpr(std::move(node));
					} else if (kind(sym) == SymbolKind::Field) {
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
						query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
							"Handling of access to method without a call is not implemented yet"
							"argument at compile time",
							current_expr->origin.getStablePosition()
						));
						return query::Failed();
					}
					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"Unsupported symbol kind in type lookup for symbol: ",
							prettyDebugPrint(sym, query_ctx)
						),
						expr_access->getName().unlock(query_ctx)->getSourcePosition().unlock(
							query_ctx
						)
					));
					// @TODO: #1412 Support lookup of other kinds of symbols in classes.
					return query::Failed();
				}
				variant_case_novalue(errors::Ambiguity) {
					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						"Accessed value is ambiguous.",
						expr_access->getName().unlock(query_ctx)->getSourcePosition().unlock(
							query_ctx
						)
					));
					return query::Failed();
				}
				variant_case_novalue(errors::SymbolNotFound) {
					// @TODO: #1472 Handle dynamic field/method names, a.k.a. access operator
					// overloads. Ex.: obj.a fails to look up 'a', but it can still call
					// obj.selectDynamic("a"). See Scala's Dynamic:
					// https://www.scala-lang.org/api/current/scala/Dynamic.html

					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						"Accessed value not found.",
						expr_access->getName().unlock(query_ctx)->getSourcePosition().unlock(
							query_ctx
						)
					));
					return query::Failed();
				}
				variant_case_novalue(errors::Inaccessible) {
					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						"Accessed value is not visible from here.",
						expr_access->getName().unlock(query_ctx)->getSourcePosition().unlock(
							query_ctx
						)
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
			const auto& lookup_result = HInterface::ofSymbol(query_ctx, namespace_like_symbol)
			                                .lookupExpectUnique(
												expr_access->getStablePosition(),
												query_ctx,
												expr_access->getName().unlock(query_ctx)->unwrap()
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
				auto lookup_qresult
					= HInterface::ofTypeInstance(current_expr_type)
				          .lookup(
							  query_ctx,
							  expr_access->getName().unlock(query_ctx)->unwrap(),
							  { .accessing_scope
				                = query_ctx.query<QueryPrimaryCodeScopeFor>({ expr_access }) }
						  );
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				// @TODO: #1412 fix dealias
				const auto callees_q_result = getCallableCandidates(lookup_result->leaves);
				UNPACK_QRESULT_MOVE(const auto& callees =, callees_q_result);

				base::Optional<base::Box<Expr>> expr{};
				if (callees.size() > 0
				    and getInterfaceElementOfMemberSym(query_ctx, callees.at(0))->isMethod()) {
					// The self expression may be passed by copy or by reference,
					// depending on whether the type is simple or composite, respectively.
					auto self_expr = current_expr->expression_type.getType().isSimple()
					                   ? std::move(current_expr)
					                   : Box<Expr>(makeBox<RefOfExpr>(
											 query_ctx,
											 current_expr->origin.generatedFrom(),
											 std::move(current_expr)
										 ));

					auto res = processMethodCall(
						query_ctx, callees, expr_access, call_expr, std::move(self_expr)
					);
					UNPACK_QRESULT_MOVE(expr =, res);
				} else {
					auto res = processFunctionCall(query_ctx, callees, expr_access, call_expr);
					UNPACK_QRESULT_MOVE(expr =, res);
				}
				return ChainState::ofExpr(std::move(expr.value()));
			}
			case lexer::Token::Square: {
				auto access_res = processPSTExpr(std::move(current_expr), expr_access);
				UNPACK_QRESULT_MOVE(auto access_state =, access_res);

				auto square_call_res = processSquareCall(
					query_ctx, std::move(access_state).asExpr(query_ctx), call_expr
				);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default: {
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
				auto lookup_qresult
					= HInterface::ofSymbol(query_ctx, namespace_like_symbol)
				          .lookup(query_ctx, expr_access->getName().unlock(query_ctx)->unwrap());
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				UNPACK_QRESULT(
					const auto& callees =, getCallableCandidatesWithBake(lookup_result, expr_access)
				);

				auto expr_result
					= processFunctionOrMethodNoSelfCall(query_ctx, callees, expr_access, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				auto access_res = processPSTExpr(namespace_like_symbol, expr_access);
				UNPACK_QRESULT_MOVE(auto access_state =, access_res);
				auto square_call_res = processSquareCall(
					query_ctx, std::move(access_state).asExpr(query_ctx), call_expr
				);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default: {
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
		 * Case when we have an access expression not followed by a call expression and the
		 * current state is a type, for example "MyClass.CONSTANT".
		 */
		auto processPSTExpr(TypeInChain type, pst::Access<pst::expr::Access> expr_access)
			-> query::QResult<ChainState> {
			auto lookup_result = HInterface::ofTypeMeta(type.type).lookupExpectUnique(
				expr_access->getStablePosition(),
				query_ctx,
				expr_access->getName().unlock(query_ctx)->unwrap(),
				{ .accessing_scope = query_ctx.query<QueryPrimaryCodeScopeFor>({ expr_access }) }
			);
			// @TODO: #1412 handle dealias expressions:
			UNPACK_QRESULT_MOVE(const auto& sym_list =, lookup_result);

			auto whole_expr_origin = pstOriginOrdered(type.origin, expr_access);
			return processNamespaceOrValue(sym_list.back(), whole_expr_origin, expr_access);
		}

		/**
		 * Case when we have an access expression followed by a call expression and the current
		 * state is a type, for example:
		 * - `MyClass.staticMethod()` - this may result in a static method overload.
		 * - `MyClass.CONSTANT_ARRAY[ix]` - this is an index access into a constant of the class.
		 */
		auto processPSTExpr(
			TypeInChain                    type,
			pst::Access<pst::expr::Access> expr_access,
			pst::Access<pst::expr::Call>   call_expr
		) -> query::QResult<ChainState> {
			switch (call_expr->getType()) {
			case lexer::Token::Round: {
				auto lookup_qresult = HInterface::ofTypeMeta(type.type).lookup(
					query_ctx,
					expr_access->getName().unlock(query_ctx)->unwrap(),
					{ .accessing_scope = query_ctx.query<QueryPrimaryCodeScopeFor>({ expr_access }) }
				);
				UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup_qresult);

				UNPACK_QRESULT(
					const auto& callees =, getCallableCandidatesWithBake(lookup_result, expr_access)
				);

				auto expr_result = processFunctionCall(query_ctx, callees, expr_access, call_expr);
				UNPACK_QRESULT_MOVE(base::Box<Expr> expr =, expr_result);
				return ChainState::ofExpr(std::move(expr));
			}
			case lexer::Token::Square: {
				auto access_res = processPSTExpr(type, expr_access);
				UNPACK_QRESULT_MOVE(auto access_state =, access_res);
				auto square_call_res = processSquareCall(
					query_ctx, std::move(access_state).asExpr(query_ctx), call_expr
				);
				UNPACK_QRESULT_MOVE(auto expr =, square_call_res);
				return ChainState::ofExpr(std::move(expr));
			}
			default: {
				query_ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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
		 * Case when we have a call expression and the current state is a type,
		 * for example "Point[3][3]".
		 *
		 * A call on a type is not a lookup in it, but an operation on the type used as a value,
		 * so the type becomes a type literal and the call is processed as on any expression.
		 */
		auto processPSTExpr(TypeInChain type, pst::Access<pst::expr::Call> call_expr)
			-> query::QResult<ChainState> {
			return processPSTExpr(
				Box<Expr>(makeBox<LiteralTypeExpr>(query_ctx, type.origin, type.type)), call_expr
			);
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
			case SymbolKind::Module: {
				return ChainState::ofNamespaceLike(symbol, pst_element_origin);
			}
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const: {
				auto expr = makeBox<IdentifierExpr>(query_ctx, pst_element_origin, symbol);
				return ChainState::ofExpr(std::move(expr));
			}
			case SymbolKind::Class: {
				auto class_type = query_ctx.query<tsh::QueryClassType>({ symbol });
				return ChainState::ofType(class_type, pst_element_origin);
			}
			case SymbolKind::Field: {
				// A static field is stored once for the whole program, so it is referred to
				// directly, the same way a global variable is, and not through a `self`.
				if (isStaticField(query_ctx, symbol)) {
					auto expr = makeBox<IdentifierExpr>(query_ctx, pst_element_origin, symbol);
					return ChainState::ofExpr(std::move(expr));
				}

				auto expr = processFieldNoSelf(query_ctx, symbol, pst_element_origin, pst_elem);
				UNPACK_QRESULT_MOVE(base::Box<Expr> field_expr =, expr);
				return ChainState::ofExpr(std::move(field_expr));
			}
			case SymbolKind::Template: {
				return ChainState::ofNamespaceLike(symbol, pst_element_origin);
			}
			default:
				query_ctx.logInt(makeBox<dia::PlaceholderError>(
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
				ctx.logInt(makeBox<dia::PlaceholderError>(
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
							ctx.logInt(makeBox<dia::PlaceholderError>(
								"Method call without `self` argument.",
								call_expr->getStablePosition()
							));
							return query::Failed();
						}
						variant_case_novalue(errors::Ambiguity) {
							ctx.logInt(makeBox<dia::PlaceholderError>(
								"Multiple candidates for `self` argument found which should be "
								"impossible.",
								call_expr->getStablePosition()
							));
							return query::Failed();
						}
						variant_case_novalue(errors::Inaccessible) {
							// `self` is looked up in a scope, and only the elements of the
							// interface of a type are ever hidden by their visibility.
							CORE_PANIC("The `self` argument cannot be inaccessible.");
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
				} else if (this->current_state.isType()) {
					return processPSTExpr(this->current_state.getType(), current_element_value);
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

			auto res = [&]() -> query::QResult<ChainState> {
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
				} else if (this->current_state.isType()) {
					return processPSTExpr(
						this->current_state.getType(), current_element_value, next_element_value
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
					query_ctx.logInt(makeBox<dia::PlaceholderError>(
						"Expected access or call expression in chain expression",
						currentElem().value()->getStablePosition()
					));
					return query::Failed();
				}
			}
			if (error.has_value()) return query::Failed();

			if (this->current_state.isExpr())
				result_sequence.push_back(this->current_state.getExpr());
			// A type that ends the chain is used as a value, so it becomes a type literal.
			if (this->current_state.isType()) {
				auto type_in_chain = this->current_state.getType();
				result_sequence.emplace_back(
					makeBox<LiteralTypeExpr>(query_ctx, type_in_chain.origin, type_in_chain.type)
				);
			}
			if (this->current_state.isNamespaceLike()) {
				auto namespace_expr = makeBox<IdentifierExpr>(
					query_ctx,
					this->current_state.getNamespaceLikePstOrigin(),
					this->current_state.getNamespaceLikeSymbol()
				);
				result_sequence.emplace_back(std::move(namespace_expr));
			}

			if (result_sequence.empty()) {
				query_ctx.logInt(makeBox<dia::PlaceholderError>(
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
