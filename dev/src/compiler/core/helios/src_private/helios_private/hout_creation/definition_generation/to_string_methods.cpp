#include "to_string_methods.hpp"

#include <helios/queries/function_queries.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryToStringMethod, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto  to_string_sym  = ctx.query<QueryGeneratedSymbol>({
                base::StrID("toString"),
                GeneratedSymbolData{ GeneratedSymbolData::ToStringMethod{ owner_type } },
            });
			const auto& to_string_decl = ctx.query<QueryDeclOfFun>(to_string_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Integral:
			case tsh::Kind::Float:
			case tsh::Kind::Char:
			case tsh::Kind::Bool:
			case tsh::Kind::Class:
			default:
				// Currently no body logic is generated for the toString method.
				// This stub just provides an empty toString method.
				break;
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&to_string_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryToStringMethod);
}
