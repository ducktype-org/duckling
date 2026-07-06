#include "list_methods.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	SymID pushMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({ .name                  = base::StrID("push"),
		                                         .generated_symbol_data = GeneratedSymbolData{
													 GeneratedSymbolData::PushMethod{ type } } });
	}

	SymID popMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({ .name                  = base::StrID("pop"),
		                                         .generated_symbol_data = GeneratedSymbolData{
													 GeneratedSymbolData::PopMethod{ type } } });
	}

	namespace {
		Box<code::Expr> buildDereffedSelf(query::Context& ctx, SymID self_symbol) {
			return makeBox<code::DerefExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_symbol)
			);
		}
	}

	struct IMPLEMENT_QUERY(QueryPushMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(pushMethodForType(ctx, key))->valueOrThrow();

			const SymID self_symbol    = decl.parameters.at(0).helios_symbol;
			const SymID element_symbol = decl.parameters.at(1).helios_symbol;

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(makeBox<code::ExprStmt>(
				code::generatedOrigin(),
				makeBox<code::ListPushExpr>(
					code::generatedOrigin(),
					buildDereffedSelf(ctx, self_symbol),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), element_symbol)
				)
			));

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPushMethod);

	struct IMPLEMENT_QUERY(QueryPopMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(popMethodForType(ctx, key))->valueOrThrow();

			const SymID self_symbol  = decl.parameters.at(0).helios_symbol;
			const SymID count_symbol = decl.parameters.at(1).helios_symbol;

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(makeBox<code::ExprStmt>(
				code::generatedOrigin(),
				makeBox<code::ListPopExpr>(
					code::generatedOrigin(),
					buildDereffedSelf(ctx, self_symbol),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), count_symbol)
				)
			));

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPopMethod);
}
