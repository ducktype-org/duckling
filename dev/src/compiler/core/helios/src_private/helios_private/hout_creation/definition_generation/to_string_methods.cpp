#include "to_string_methods.hpp"

#include "helios/queries/function_queries.hpp"
#include "helios_private/symbols/symbols.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryToStringMethod, HOUTFunction) {
		std::vector<Box<code::Stmt>> bodyForUnit() {
			// @TODO: ???
		}

		static auto provide(Context& ctx, const tsh::AbstractType owner_type) -> PResult {
			const auto to_string_sym = ctx.query<QueryGeneratedSymbol>({
				base::StrID("toString"),
				GeneratedSymbolData{ GeneratedSymbolData::ToStringMethod{ owner_type } },
			});
			const auto to_string_decl = ctx.query<QueryDeclOfFun>(to_string_sym);

			switch (owner_type.getKind()) {
			case tsh::Kind::Unit:
				// @TODO: ???
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryToStringMethod)
}
