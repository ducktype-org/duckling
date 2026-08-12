#include "global_data_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryHOUTGlobalData, query::QResult<HOUTGlobalData>) {
		static auto provide(Context& ctx, QKey symbol) -> PResult {
			auto symbol_kind = kind(symbol);
			CORE_ASSERT(
				symbol_kind == SymbolKind::Const
					|| (symbol_kind == SymbolKind::Variable && isGlobalVar(ctx, symbol)),
				"QueryHOUTGlobalData expects a global const/variable symbol"
			);

			auto data_type = symbol_kind == SymbolKind::Const ? HOUTGlobalDataType::Constant
			                                                  : HOUTGlobalDataType::Variable;

			const auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow();

			auto maybe_pst_decl = maybeSymbolPst(symbol);
			auto origin         = [&]() -> code::ElementOrigin {
                if (maybe_pst_decl.has_value()) {
                    auto pst_decl = maybe_pst_decl.value().unlock(ctx);
                    return code::pstOrigin(pst_decl);
                } else {
                    // Note: we could generate better origin upon const creation and use it here,
                    // if we ever needed to
                    return code::generatedOrigin();
                }
			}();

			auto value = [&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
				switch (data_type) {
				case HOUTGlobalDataType::Variable: {
					CORE_ASSERT(
						maybe_pst_decl.has_value(),
						"Global variable symbol without PST Implemented Semantics is not handled "
						"in QueryHOUTGlobalData"
					);

					auto var_decl = stmt(ctx, symbol)->dynamicCast<pst::Variable>().value();

					auto get_initial_value = [&]() -> BoxOrCRef<code::Expr> {
						if (auto maybe_initial_pst = var_decl->getValue()) {
							auto initial_value_pst
								= maybe_initial_pst.value().unlock(ctx)->getExpr();
							return getHoutOfExprWithExpectedType(
									   ctx,
									   initial_value_pst,
									   symbol_type,
									   var_decl->getName().unlock(ctx)->getStablePosition()
							)
							    .valueOrThrow();
						}

						return defgen::getDefaultInitializerExpr(
								   ctx, symbol_type, var_decl->getStablePosition()
						)
						    .valueOrThrow();
					};
					auto initial_value = get_initial_value();

					return HOUTGlobalVariable{ std::move(initial_value) };
				}
				case HOUTGlobalDataType::Constant:
					return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).valueOrThrow() };
				default:
					CORE_PANIC("Unhandled HOUTGlobalDataType");
				}
			}();

			return HOUTGlobalData{
				.helios_symbol = symbol,
				.origin        = origin,
				.original_name = name(symbol),
				.data_type     = data_type,
				.value         = std::move(value),
				.type          = symbol_type,
			};
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHOUTGlobalData);

	Box<code::Expr> getMIRConstructorExpr(query::Context& ctx, CRef<HOUTGlobalData> global_data) {
		using namespace code::shorthands;
		Shorthand s(ctx);

		CORE_ASSERT(
			v_matches(global_data->value, HOUTGlobalVariable),
			"Call only valid with global variable."
		);

		CRef<code::Expr> initial_value_expr
			= v_get(global_data->value, HOUTGlobalVariable).initial_value.ref();

		auto move_in_symbol = moveInSymForType(ctx, global_data->type);
		return s.call(
			s.ident(move_in_symbol),
			s.ptrOf(s.ident(global_data->helios_symbol)),
			initial_value_expr->clone()
		);
	}

	base::Optional<Box<code::Expr>> getMIRDestructorExpr(
		query::Context& ctx, CRef<HOUTGlobalData> global_data
	) {
		using namespace code::shorthands;
		Shorthand s(ctx);
		auto      destructor = getTypeDestructor(ctx, global_data->type);
		if (destructor.empty()) return {};

		// refOf here is intentional, for `box T` reference types
		// we the type to `ref T` and the MIR will remove additional address of.
		// And for directy types it will just add address of.
		return s.call(s.ident(destructor.value()), s.refOf(s.ident(global_data->helios_symbol)));
	}
}
