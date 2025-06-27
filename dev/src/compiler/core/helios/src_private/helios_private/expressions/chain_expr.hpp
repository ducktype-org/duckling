#include "helios/hout/visitors.hpp"
#include "helios_private/expressions/query_hout_of_expr.hpp"
#include "pst_parser/access.hpp"
#include "pst_parser/elements/hierarchy/expressions/call.hpp"
#include "pst_parser/elements/hierarchy/expressions/identifier_literal.hpp"
#include "query_framework/context.hpp"
#include "query_framework/query_result.hpp"

#include <helios/hout/elements/expr.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>

#include "base/optional.hpp"
#include "base/variant.hpp"

namespace compiler::helios::code {

	struct HoutExprSymbolVisitor final: public HoutExprVisitorPanicky {
		explicit HoutExprSymbolVisitor(query::Context& ctx): ctx(ctx) {}

		query::Context& ctx;

		base::Optional<SymID> symbol;

		void visitIdentifierExpr(const IdentifierExpr& val) override { symbol = val.symbol; }
	};

	base::Optional<SymID> getExpressionSymID(query::Context& ctx, CRef<code::Expr> expr) {
		HoutExprSymbolVisitor visitor(ctx);
		expr->acceptVisitor(visitor);
		return visitor.symbol;
	}

	struct ChainContext {
		base::MBox<Expr> expr{};
        base::Optional<SymID> namespace_id{};

		[[nodiscard]] bool isNamespace() const { return namespace_id.has_value(); }

		[[nodiscard]] bool isExpr() const {
            return expr.toOpt().has_value();
		}

		[[nodiscard]] bool isEmpty() const { return not isNamespace() and not isExpr(); }


		[[nodiscard]] auto getExpr() -> base::Box<Expr> {
            return std::move(expr).toOptBox().value();
		}

        [[nodiscard]] auto getNamespace() -> SymID {
            return namespace_id.value();
        }

        ChainContext(base::Box<Expr> expr) 
            : expr(std::move(expr)), namespace_id(base::Optional<SymID>{}) {}

        ChainContext(SymID namespace_id)
            : expr(base::MBox<Expr>{}), namespace_id(namespace_id) {}
    
    private:
        ChainContext() 
            : expr(base::MBox<Expr>{}), namespace_id(base::Optional<SymID>{}) {}

        friend struct ChainExprConstruction;
	};

    using HandlerOutput = std::tuple<ChainContext, base::Optional<base::Box<Expr>>>;

	auto handlePSTExpr(
		query::Context&   ctx,
		base::Box<Expr>   current_expr,
		pst::expr::Access expr_access,
		pst::expr::Call   call_expr
	) -> query::QResult<HandlerOutput, errors::Failed> {
		// perform lookup in type, may result in method, method parameter overload
	}

	auto handlePSTExpr(
		query::Context&   ctx,
		SymID             namespace_id,
		pst::expr::Access expr_access,
		pst::expr::Call   call_expr
	) -> query::QResult<HandlerOutput, errors::Failed> {
		// perform lookup in namespace, usual functon, function parameter overload
	}

	auto handlePSTExpr(
		query::Context& ctx,
		ChainContext::Empty,
		pst::expr::IdentifierLiteral ident,
		pst::expr::Call              call_expr
	) -> query::QResult<HandlerOutput, errors::Failed> {
		// Implementation same as in handlePSTExpr(query::Context& ctx, SymID namespace_id,
		// pst::expr::Access expr_access, pst::expr::Call call_expr but lookup is performed global
	}

	auto handlePSTExpr(query::Context& ctx, ChainContext::Empty, pst::expr::IdentifierLiteral ident)
		-> query::QResult<HandlerOutput, errors::Failed> {
		// Lookup global for const/variables/namespaces. Depending on the type of found identifier
		// it will return ChainContext with namespace or expr.
	}

	auto handlePSTExpr(query::Context& ctx, base::Box<Expr> current_expr, pst::expr::Call call_expr)
		-> query::QResult<HandlerOutput, errors::Failed> {
		// perform lookup in type, but before in the chain there was not a "access expression",
		/// so it was "call expression", meaning we call a function returned by the previous function
	}

	auto handlePSTExpr(query::Context& ctx, SymID namespace_id, pst::expr::Call call_expr)
		-> query::QResult<HandlerOutput, errors::Failed> {
		// error, Namespace() encountered
	}

	auto handlePSTExpr(
		query::Context& ctx, base::Box<Expr> current_expr, pst::expr::Access expr_access
	) -> query::QResult<HandlerOutput, errors::Failed> {
		// perform lookup in type
	}

	auto handlePSTExpr(query::Context& ctx, SymID namespace_id, pst::expr::Access expr_access)
		-> query::QResult<HandlerOutput, errors::Failed> {
		// perform lookup in namespace
	}

	struct ChainExprConstruction {
		ChainContext                                     current_context{};
		std::vector<base::Box<Expr>>                     result_sequence{};
		std::vector<pst::AccessLocked<pst::ExprElement>> chain_elements{};
		size_t                                           i{ 0 };

		ChainExprConstruction(
			pst::AccessLocked<pst::expr::ChainExpr>          atom,
			std::vector<pst::AccessLocked<pst::ExprElement>> chain_elements
		) {
			this->chain_elements.emplace_back(atom);
			this->chain_elements.insert(
				this->chain_elements.end(),
				std::make_move_iterator(chain_elements.begin()),
				std::make_move_iterator(chain_elements.end())
			);
		}

		bool isNextElementCall(query::Context& ctx) const {
			if (i + 1 >= chain_elements.size()) return false;
			return chain_elements[i + 1].unlock(ctx).dynamicCast<pst::expr::Call>().has_value();
		}

        bool isCurrentElementAccess(query::Context& ctx) const {
            return chain_elements[i].unlock(ctx).dynamicCast<pst::expr::Access>().has_value();
        }

        bool isCurrentElementCall(query::Context& ctx) const {
            return chain_elements[i].unlock(ctx).dynamicCast<pst::expr::Call>().has_value();
        }

        bool isCurrentElementIdentifier(query::Context& ctx) const {
            return chain_elements[i].unlock(ctx).dynamicCast<pst::expr::IdentifierLiteral>().has_value();
        }

		query::QResult<base::Box<Expr>, errors::Failed> run(query::Context& ctx) {
            if (not isCurrentElementIdentifier(ctx)) {
                ctx.log(
                    dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
                        chain_elements[i].unlock(ctx)->getSourcePosition(),
                        "Expected identifier literal in as first element in chain expression"
                    )
                );
                return query::QError(errors::Failed());
            }

            if (isNextElementCall(ctx)) {
                step(ctx, chain_elements[i].unlock(ctx), chain_elements[i + 1].unlock(ctx));
            }
            else {
                step(ctx, chain_elements[i].unlock(ctx));
            }

			for (size_t i{ 0 }; i < chain_elements.size(); ++i) {
				if (isCurrentElementAccess(ctx) && isNextElementCall(ctx)) {
					step(ctx, chain_elements[i].unlock(ctx), chain_elements[i + 1].unlock(ctx));
					i++;  // skip next element, because it is handled
				} else if (isCurrentElementAccess(ctx))
					step(ctx, chain_elements[i].unlock(ctx));
				} else if (isCurrentElementCall(ctx)) {
                    step(ctx, chain_elements[i].unlock(ctx));
                }
                else {
                    ctx.log(
                        dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Parser>::make(
                            chain_elements[i].unlock(ctx)->getSourcePosition(),
                            "Expected access or call expression in chain expression"
                        )
                    );
                    return query::QError(errors::Failed());
                }
			}
	};

	ExprConstructionResult fromChainExpr(
		query::Context& ctx, pst::Access<pst::expr::ChainExpr> expr
	) {
		base::Optional<SymID>           current_symbol;
		base::Optional<base::Box<Expr>> current_expr;
		base::Optional<base::Box<Expr>> result;

		auto                                          x     = expr->getAtom();
		auto                                          chain = expr->getChain();
		base::Optional<pst::Access<pst::ExprElement>> next
			= (chain.size() > 0) ? chain[0].unlock(ctx)
		                         : base::Optional<pst::Access<pst::ExprElement>>{};

		if (next is call) {
			step(ctx, current_expr, current_symbol, x, next.value());
			i++;
		} else {
			step(ctx, current_expr, current_symbol, x);
		}
		for (size_t i = 0; i < chain.size(); ++i) {
			if (next is call) {
				step(ctx, current_expr, current_symbol, x, next.value());
				i++;
			} else {
				step(ctx, current_expr, current_symbol, x);
			}
		}

		for (auto el: expr->getChain()) {
			if (auto pst_access_opt = el.unlock(ctx).dynamicCast<pst::expr::Access>()) {
				// ...
			}
			if (auto pst_call_opt = el.unlock(ctx).dynamicCast<pst::expr::Call>()) {
				// ...
			}
		}
	}
}
