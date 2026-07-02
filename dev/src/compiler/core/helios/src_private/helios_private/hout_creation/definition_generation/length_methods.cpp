#include "length_methods.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	SymID lengthMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>(
			{ .name                  = base::StrID("length"),
		      .generated_symbol_data = Method{ type, GeneratedMethodType::LengthMethod } }
		);
	}

	struct IMPLEMENT_QUERY(QueryLengthMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(lengthMethodForType(ctx, key))->valueOrThrow();
			auto  slice_fields = ctx.query<QuerySliceTypeData>(key);
			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::AccessExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), decl.parameters.at(0).helios_symbol
					),
					slice_fields->len
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLengthMethod);
}
