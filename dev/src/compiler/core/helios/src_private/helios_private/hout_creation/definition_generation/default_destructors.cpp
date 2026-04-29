#include "default_destructors.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryDefaultDestructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto dtor_sym = ctx.query<QueryGeneratedSymbol>({
				base::StrID("__destruct"),
				GeneratedSymbolData{ GeneratedSymbolData::DefaultDestructor{ owner_type } },
			});
			const auto& dtor_decl = ctx.query<QueryDeclOfFun>(dtor_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Class:
			default:
				// Currently no body logic is generated for the default destructor.
				// This stub just provides an empty destructor.
				break;
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&dtor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultDestructor);
}
