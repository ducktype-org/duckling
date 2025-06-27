
#include "chain_expr.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>
#include <token_parser_core/common_elements.hpp>
#include <typesystem/higher/types.hpp>

#include "base/box.hpp"
#include "base/exceptions.hpp"
#include <base/optional.hpp>

#include <tuple>

namespace compiler::helios::code {
	/**
	 * @brief State for the building of the chain expression in HOUT.
	 * It mainly stores two distinct states:
	 * - namespace state, for example after processing "foo().Namespace"
	 * - expression state, for example after processing "foo().bar[20]"
	 */
	struct ChainState {
		base::MBox<Expr>      expr{};
		base::Optional<SymID> namespace_id{};

		[[nodiscard]] bool isNamespace() const { return namespace_id.has_value(); }

		[[nodiscard]] bool isExpr() const { return expr.toOpt().has_value(); }

		[[nodiscard]] bool isEmpty() const { return not isNamespace() and not isExpr(); }

		[[nodiscard]] auto getExpr() -> base::Box<Expr> {
			return std::move(expr).toOptBox().value();
		}

		[[nodiscard]] auto getNamespace() -> SymID { return namespace_id.value(); }

		static ChainState ofExpr(base::Box<Expr> expr) { return { std::move(expr) }; }

		static ChainState ofNamespace(SymID namespace_id) { return { namespace_id }; }

	private:
		ChainState(): expr(base::MBox<Expr>{}), namespace_id(base::Optional<SymID>{}) {}

		ChainState(base::Box<Expr> expr):
			  expr(std::move(expr)),
			  namespace_id(base::Optional<SymID>{}) {}

		ChainState(SymID namespace_id): expr(base::MBox<Expr>{}), namespace_id(namespace_id) {}

		friend class ChainExprConstruction;
	};

	/**
	 * @brief Constructs an expression from a PST chain expression, for example from
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
	 */
	class ChainExprConstruction {
		/**
		 * @brief Resulting sequence of expressions.
		 * If at the end there will be a single expression, it will be the result of the chain.
		 */
		std::vector<base::Box<Expr>> result_sequence{};
		/**
		 * @brief All chain elements that would be processed.
		 */
		std::vector<pst::AccessLocked<pst::ExprElement>> chain_elements{};
		/**
		 * @brief As an optimization we keep unlocked chain elements in a separate vector, to avoid
		 * unlocking them multiple times.
		 */
		std::vector<pst::Access<pst::ExprElement>> chain_elements_unlocked{};

		/**
		 * @brief The temporaty buffor for the currently built value from left to current place of
		 the chain. So for example after processing "a.b.c" it will contain hout expr:
		 "access(access(a, field=b), field=c))"".
		 */
		ChainState current_context{};
		/**
		 * @brief Index in the chain currently being processed.
		 */
		size_t index{ 0 };

		base::Optional<pst::Access<pst::ExprElement>> currentElem(query::Context& ctx);

		base::Optional<pst::Access<pst::ExprElement>> nextElem(query::Context& ctx);

		template<typename T>
		bool isNextElement(query::Context& ctx);

		template<typename T>
		bool isCurrentElement(query::Context& ctx);

		// =============================== MAIN PROCESSING FUNCTIONS ===============================

		using ProcessPSTExprOutput = std::tuple<ChainState, base::Optional<base::Box<Expr>>>;

		/**
		 * @brief Given first identifier in a chain perform lookup in the current scope.
		 * Returns a chain state with namespace or identifier expression based on the lookup result.
		 */
		auto processPSTExpr(query::Context& ctx, pst::Access<pst::expr::IdentifierLiteral> ident)
			-> query::QResult<ProcessPSTExprOutput, errors::Failed>;


		/**
		 * @brief Given first identifier in a chain and a call expression, perform function lookup
		 * in the current scope with overloading in the future and return a call expression.
		 */
		auto processPSTExpr(
			query::Context&                           ctx,
			pst::Access<pst::expr::IdentifierLiteral> ident,
			pst::Access<pst::expr::Call>              call_expr
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		/**
		 * @brief Call in situations were we don't have "access expr" then "call expr", for example
		 * a[i]() or b()()
		 */
		auto processPSTExpr(query::Context&, base::Box<Expr>, pst::Access<pst::expr::Call>)
			-> query::QResult<ProcessPSTExprOutput, errors::Failed> {
			throw base::NotYetImplemented("Helios chain expr: call without access before it");
		}

		/**
		 * @brief Call on namespace, for example Namespace()
		 */
		auto processPSTExpr(
			query::Context& ctx, SymID namespace_id, pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		/**
		 * @brief Access on the expression, for example access on field "c"  in "x().c" or "x.y.c"
		 * Should check if accessed field is a namespace or not.
		 */
		auto processPSTExpr(
			query::Context&                ctx,
			base::Box<Expr>                current_expr,
			pst::Access<pst::expr::Access> expr_access
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		/**
		 * @brief Search for value given by identifier literal in the namespace.
		 */
		auto processPSTExpr(
			query::Context& ctx, SymID namespace_id, pst::Access<pst::expr::Access> expr_access
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		/**
		 * @brief Given access expr and subsequent call expression, perform lookup in type given by
		 * previous expression, like "my_expr[20].foo()". This may result in a method. Method
		 * parameter overload is possible.
		 */
		auto processPSTExpr(query::Context&, base::Box<Expr>, pst::Access<pst::expr::Access>, pst::Access<pst::expr::Call>)
			-> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		/**
		 * @brief Given access expr and subsequent call expression, perform lookup in the namespace
		 * given by previous expression and return a call expression. Function overload is possible.
		 */
		auto processPSTExpr(
			query::Context&                ctx,
			SymID                          namespace_id,
			pst::Access<pst::expr::Access> expr_access,
			pst::Access<pst::expr::Call>   call_expr
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		// ======================== MAIN PROCESSING FUNCTIONS HELPERS ========================

		auto processFunctionCall(
			query::Context&              ctx,
			CRef<LookupResult>           lookup_result,
			const tpc::Identifier&       name,
			pst::Access<pst::expr::Call> call_expr
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		auto processNamespaceOrValue(
			query::Context&                                   ctx,
			const query::QResult<SymbolList, errors::Failed>& lookup_result,
			const tpc::Identifier&                            name
		) -> query::QResult<ProcessPSTExprOutput, errors::Failed>;

		// =============================== MAIN PROCESSING LOOP ===============================

		/**
		 * @brief Perform a procesing step on the current element of the chain.
		 * @warning It assumes that the current element exist.
		 */
		template<typename T>
		base::Optional<errors::Failed> step(query::Context& ctx);

		/**
		 * @brief Perform a procesing step on current element of the chain and next one.
		 * @warning It assumes that the current element and next one exist.
		 */
		template<typename T1, typename T2>
		base::Optional<errors::Failed> step(query::Context& ctx);

		/**
		 * @brief Same as above, but it performs a step on the first element of the chain, where chain state is empty.
		 */
		template<typename T>
		base::Optional<errors::Failed> firstStep(query::Context& ctx);

		/**
		 * @brief Same as above, but it performs a step on the first element of the chain, where chain state is empty.
		 */
		template<typename T1, typename T2>
		base::Optional<errors::Failed> firstStep(query::Context& ctx);

	public:
		ChainExprConstruction(pst::Access<pst::expr::ChainExpr> expr);

		/**
		 * @brief Main function of the ChainExprConstruction with the loop.
		 * Performs the construction of the chain expression from the chain elements.
		 */
		query::QResult<base::Box<Expr>, errors::Failed> run(query::Context& ctx);
	};

	auto ChainExprConstruction::processNamespaceOrValue(
		query::Context&                                   ctx,
		const query::QResult<SymbolList, errors::Failed>& lookup_result,
		const tpc::Identifier&                            name
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		if (!lookup_result) {
			ctx.log(makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
				name.position, base::strConcat("Value '", name.value, "' not found")
			));
			return query::QError(errors::Failed());
		}

		const auto& sym = lookup_result.value().back();
		switch (kind(sym)) {
		case SymbolKind::Namespace: {
			// Namespace found, return ChainContext with namespace
			return { ProcessPSTExprOutput{ ChainState::ofNamespace(sym),
				                           base::Optional<base::Box<Expr>>{} } };
		}
		case SymbolKind::Variable:
		case SymbolKind::Parameter:
		case SymbolKind::Const:
		case SymbolKind::Class: {
			// Variable or constant found, return ChainContext with expr
			auto expr = makeBox<IdentifierExpr>(ctx, sym);
			return { ProcessPSTExprOutput{ ChainState::ofExpr(std::move(expr)),
				                           base::Optional<base::Box<Expr>>{} } };
		}
		default:
			return query::QError(errors::Failed());
		}
	}

	auto ChainExprConstruction::processFunctionCall(
		query::Context&              ctx,
		CRef<LookupResult>           lookup_result,
		const tpc::Identifier&       name,
		pst::Access<pst::expr::Call> call_expr
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		if (lookup_result->isEmpty()) return query::QError(errors::Failed());
		auto callee = lookup_result->leaves.back();

		std::vector<Box<Expr>> call_arguments;
		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			auto arg_expr = ctx.query<QueryHoutOfExpr>({ arg.unlock(ctx)->getExpr() });
			if (arg_expr.hasError()) return query::QError(errors::Failed());
			call_arguments.emplace_back(std::move(arg_expr.value()));
		}

		auto call_type_result = ctx.query<QueryTypeOfSymbol>({ callee });
		if (call_type_result->hasError()) return query::QError(errors::Failed());
		tsh::SymbolType<tsh::FunctionAbstractType> call_type = call_type_result->value();

		if (call_type.getType().getParameterTypes().size() != call_arguments.size()) {
			ctx.log(makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
				name.position, "Invalid number of arguments"
			));
			return query::QError(errors::Failed());
		}

		std::vector<base::Box<Expr>> coerced_arguments;
		for (size_t i = 0; i < call_type.getType().getParameterTypes().size(); ++i) {
			auto coerced = coerceExpression(
				std::move(call_arguments[i]), call_type.getType().getParameterTypes()[i]
			);
			if (coerced.hasError()) {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						name.position, "Invalid argument type"
					)
				);
				return query::QError(errors::Failed());
			}
			coerced_arguments.emplace_back(std::move(coerced.value()));
		}

		auto node = makeBox<CallExpr>(ctx, callee, std::move(coerced_arguments));
		return { ProcessPSTExprOutput{ ChainState::ofExpr(std::move(node)),
			                           base::Optional<base::Box<Expr>>{} } };
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context& ctx, pst::Access<pst::expr::IdentifierLiteral> ident
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		// Lookup global for const/variables/namespaces. Depending on the type of found
		// identifier it will return ChainContext with namespace or expr.
		auto        scope         = ctx.query<QueryPrimaryCodeScopeFor>({ ident });
		const auto& lookup_result = HInterface::ofScopeWithParents(scope).lookupExpectUnique(
			ident->getName().position, ctx, ident->getName().value
		);
		return processNamespaceOrValue(ctx, lookup_result, ident->getName());
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context&                           ctx,
		pst::Access<pst::expr::IdentifierLiteral> ident,
		pst::Access<pst::expr::Call>              call_expr
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		if (call_expr->getType() != lexer::Token::Round) {
			throw base::NotYetImplemented(
				base::strConcat("HOUT call with invalid bracket type: ", char(call_expr->getType()))
			);
		}
		auto scope = ctx.query<QueryPrimaryCodeScopeFor>({ ident });
		auto lookup_result
			= HInterface::ofScopeWithParents(scope).lookup(ctx, ident->getName().value);

		if (lookup_result->isEmpty()) return query::QError(errors::Failed());
		return processFunctionCall(ctx, lookup_result, ident->getName(), call_expr);
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context& ctx, SymID, pst::Access<pst::expr::Call> call_expr
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		ctx.log(makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>>(
			call_expr->getSourcePosition(), base::strConcat("Namespace is not callable")
		));
		return query::QError(errors::Failed());
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context& ctx, base::Box<Expr> current_expr, pst::Access<pst::expr::Access> expr_access
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		// @TODO for now it is a mock as we don't have lookup in type instance and proper helios
		// access expr.
		auto current_expr_type = current_expr->expression_type.getType();
		auto lookup_result     = HInterface::ofTypeInstance(current_expr_type)
		                         .lookup(ctx, expr_access->getName().value);

		const auto& looked_up_symbols = lookup_result->leaves;
		if (looked_up_symbols.empty()) {
			auto node
				= makeBox<AccessExpr>(ctx, std::move(current_expr), expr_access->getName().value);
			return { ProcessPSTExprOutput{ ChainState::ofExpr(std::move(node)),
				                           base::Optional<base::Box<Expr>>{} } };
		} else {
			auto sym = looked_up_symbols.back();
			if (kind(sym) == SymbolKind::Namespace) {
				return { ProcessPSTExprOutput{ ChainState::ofNamespace(sym),
					                           std::move(current_expr) } };
			}
			return query::QError(errors::Failed());
		}
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context& ctx, SymID namespace_id, pst::Access<pst::expr::Access> expr_access
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		const auto& lookup_result
			= HInterface::ofSymbol(namespace_id)
		          .lookupExpectUnique(
					  expr_access->getSourcePosition(), ctx, expr_access->getName().value
				  );
		return processNamespaceOrValue(ctx, lookup_result, expr_access->getName());
	}

	auto ChainExprConstruction::processPSTExpr(
		query::Context&                ctx,
		SymID                          namespace_id,
		pst::Access<pst::expr::Access> expr_access,
		pst::Access<pst::expr::Call>   call_expr
	) -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		auto lookup_result
			= HInterface::ofSymbol(namespace_id).lookup(ctx, expr_access->getName().value);
		return processFunctionCall(ctx, lookup_result, expr_access->getName(), call_expr);
	}

	auto ChainExprConstruction::
		processPSTExpr(query::Context&, base::Box<Expr>, pst::Access<pst::expr::Access>, pst::Access<pst::expr::Call>)
			-> query::QResult<ProcessPSTExprOutput, errors::Failed> {
		throw base::NotYetImplemented("Helios chain expr: call on access expr not implemented yet");
		return query::QError(errors::Failed());
	}

	ChainExprConstruction::ChainExprConstruction(pst::Access<pst::expr::ChainExpr> expr) {
		this->chain_elements.emplace_back(expr->getAtom());
		auto chain           = expr->getChain();
		this->chain_elements = { chain.begin(), chain.end() };
		this->chain_elements_unlocked.reserve(this->chain_elements.size());
	}

	base::Optional<pst::Access<pst::ExprElement>> ChainExprConstruction::currentElem(
		query::Context& ctx
	) {
		if (index >= chain_elements.size()) return {};

		for (size_t j{ chain_elements_unlocked.size() }; j <= index; ++j)
			chain_elements_unlocked.push_back(chain_elements[j].unlock(ctx));
		return chain_elements_unlocked[index];
	}

	base::Optional<pst::Access<pst::ExprElement>> ChainExprConstruction::nextElem(query::Context& ctx
	) {
		auto next_index = index + 1;
		if (next_index >= chain_elements.size()) return {};

		for (size_t j{ chain_elements_unlocked.size() }; j <= next_index; ++j)
			chain_elements_unlocked.push_back(chain_elements[j].unlock(ctx));

		return chain_elements_unlocked[next_index];
	}

	template<typename T>
	bool ChainExprConstruction::isNextElement(query::Context& ctx) {
		auto elem = nextElem(ctx);
		if (elem.empty()) return false;
		return elem.value().template dynamicCast<T>().has_value();
	}

	template<typename T>
	bool ChainExprConstruction::isCurrentElement(query::Context& ctx) {
		auto elem = currentElem(ctx);
		if (elem.empty()) return false;
		return elem.value().template dynamicCast<T>().has_value();
	}

	template<typename T>
	base::Optional<errors::Failed> ChainExprConstruction::step(query::Context& ctx) {
		auto current_element_value = currentElem(ctx).value().dynamicCast<T>().value();

		auto res = [&]() -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
			if (current_context.isExpr()) {
				auto expr = current_context.getExpr();
				return processPSTExpr(ctx, std::move(expr), current_element_value);
			} else if (current_context.isNamespace()) {
				auto namespace_id = current_context.getNamespace();
				return processPSTExpr(ctx, namespace_id, current_element_value);
			}
			return query::QError(errors::Failed());
		}();
		if (res.hasError()) return res.error();
		auto [next_context, built_expr_opt] = std::move(res.value());
		current_context                     = std::move(next_context);
		if (built_expr_opt.has_value())
			result_sequence.push_back(std::move(built_expr_opt.value()));
		return {};
	}

	template<typename T1, typename T2>
	base::Optional<errors::Failed> ChainExprConstruction::step(query::Context& ctx) {
		auto current_element_value = currentElem(ctx).value().dynamicCast<T1>().value();
		auto next_element_value    = nextElem(ctx).value().dynamicCast<T2>().value();
		auto res                   = [&]() -> query::QResult<ProcessPSTExprOutput, errors::Failed> {
            if (current_context.isExpr()) {
                auto expr = current_context.getExpr();
                return processPSTExpr(
                    ctx, std::move(expr), current_element_value, next_element_value
                );
            } else if (current_context.isNamespace()) {
                auto namespace_id = current_context.getNamespace();
                return processPSTExpr(ctx, namespace_id, current_element_value, next_element_value);
            }
            return query::QError(errors::Failed());
		}();
		if (res.hasError()) return res.error();
		auto [next_context, built_expr_opt] = std::move(res.value());
		current_context                     = std::move(next_context);
		if (built_expr_opt.has_value())
			result_sequence.push_back(std::move(built_expr_opt.value()));
		return {};
	}

	template<typename T>
	base::Optional<errors::Failed> ChainExprConstruction::firstStep(query::Context& ctx) {
		auto current_element_value = currentElem(ctx).value().dynamicCast<T>().value();

		auto res = processPSTExpr(ctx, current_element_value);
		if (res.hasError()) return res.error();
		auto [next_context, built_expr_opt] = std::move(res.value());
		current_context                     = std::move(next_context);
		if (built_expr_opt.has_value())
			result_sequence.push_back(std::move(built_expr_opt.value()));
		return {};
	}

	template<typename T1, typename T2>
	base::Optional<errors::Failed> ChainExprConstruction::firstStep(query::Context& ctx) {
		auto current_element_value = currentElem(ctx).value().dynamicCast<T1>().value();
		auto next_element_value    = currentElem(ctx).value().dynamicCast<T2>().value();
		auto res                   = processPSTExpr(ctx, current_element_value, next_element_value);
		if (res.hasError()) return res.error();
		auto [next_context, built_expr_opt] = std::move(res.value());
		current_context                     = std::move(next_context);
		if (built_expr_opt.has_value())
			result_sequence.push_back(std::move(built_expr_opt.value()));
		return {};
	}

	query::QResult<base::Box<Expr>, errors::Failed> ChainExprConstruction::run(query::Context& ctx) {
		base::Optional<errors::Failed> error{};
		index = 0;

		if (not isCurrentElement<pst::expr::IdentifierLiteral>(ctx)) {
			ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
				currentElem(ctx).value()->getSourcePosition(),
				"Expected identifier literal in as first element in chain expression"
			));
			return query::QError(errors::Failed());
		}
		if (isNextElement<pst::expr::Call>(ctx)) {
			error = firstStep<pst::expr::IdentifierLiteral, pst::expr::Call>(ctx);
			index++;
		} else
			error = firstStep<pst::expr::IdentifierLiteral>(ctx);


		while (not error.has_value() && index < chain_elements.size()) {
			if (isCurrentElement<pst::expr::Access>(ctx) && isNextElement<pst::expr::Call>(ctx)) {
				error = step<pst::expr::Access, pst::expr::Call>(ctx);
				index++;  // skip next element, because it is handled
			} else if (isCurrentElement<pst::expr::Access>(ctx))
				error = step<pst::expr::Access>(ctx);

			else if (isCurrentElement<pst::expr::Call>(ctx))
				error = step<pst::expr::Call>(ctx);

			else {
				ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
					currentElem(ctx).value()->getSourcePosition(),
					"Expected access or call expression in chain expression"
				));
				return query::QError(errors::Failed());
			}
			index++;
		}
		if (current_context.isExpr()) result_sequence.push_back(current_context.getExpr());

		if (error.has_value()) return query::QError(error.value());
		if (result_sequence.empty()) {
			ctx.log(dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
				chain_elements[0].unlock(ctx)->getSourcePosition(), "Chain expression is empty"
			));
			return query::QError(errors::Failed());
		}

		if (result_sequence.size() == 1) return std::move(result_sequence[0]);

		// If we have multiple expressions, we need to create a chain expression
		auto chain_expr = makeBox<SequenceExpr>(ctx, std::move(result_sequence));
		return chain_expr;
	}

	ExprConstructionResult fromChainExpr(
		query::Context& ctx, pst::Access<pst::expr::ChainExpr> expr
	) {
		ChainExprConstruction construction(expr);
		return construction.run(ctx);
	}
}
