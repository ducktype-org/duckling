#include "global_data_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
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
			auto       origin      = code::pstOrigin(stmt(ctx, symbol).value());

			auto value = [&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
				switch (data_type) {
				case HOUTGlobalDataType::Variable: {
					auto var_decl = stmt(ctx, symbol)->dynamicCast<pst::Variable>().value();

					auto get_initial_value = [&]() -> BoxOrCRef<code::Expr> {
						if (auto maybe_initial_pst = var_decl->getValue()) {
							auto initial_value_pst
								= maybe_initial_pst.value().unlock(ctx)->getExpr();
							return getHoutOfExprWithExpectedType(ctx, initial_value_pst, symbol_type)
							    .valueOrThrow();
						}

						return defgen::getDefaultInitializerExpr(
								   ctx, symbol_type, origin.getSourcePosition(ctx).value()
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
}
