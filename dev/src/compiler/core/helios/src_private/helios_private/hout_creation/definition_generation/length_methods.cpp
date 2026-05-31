#pragma once

#include "length_methods.hpp"

#include "helios/hout/elements/stmt.hpp"
#include "helios/queries/function_queries.hpp"
#include "helios_private/symbols/generated_symbol_data.hpp"
#include "helios_private/symbols/symbols.hpp"

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	SymID lengthMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({ .name                  = base::StrID("len"),
		                                         .generated_symbol_data = GeneratedSymbolData{
													 GeneratedSymbolData::LengthMethod{ type } } });
	}

	struct IMPLEMENT_QUERY(QueryLengthMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(lengthMethodForType(ctx, key))->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};
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
}
