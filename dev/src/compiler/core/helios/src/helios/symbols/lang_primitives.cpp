#include "lang_primitives.hpp"

#include "diagnostic_interactive/placeholder.hpp"
#include "frontend/module_tree/queries.hpp"
#include "helios/symbols/symbol_id_utils.hpp"
#include "helios_private/scopes/scopes.hpp"

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryLanguagePrimitiveSymID, query::QResult<SymID>) {
		static query::QResult<SymID> lookupPrimitive(
			query::Context&          ctx,
			LanguagePrimitive        primitive,
			std::string              package_name,
			std::vector<std::string> path,
			std::string              element_name
		) {
			std::vector<base::StrID> path_str_ids
				= path | std::views::transform([](const std::string& s) { return base::StrID(s); })
			    | std::ranges::to<std::vector>();
			auto module_opt
				= frontend::getModuleByAbsolutePath(ctx, base::StrID(package_name), path_str_ids);
			if_opt_none(module_opt) {
				std::string module_path_str = package_name + "." + base::strJoin(path, ".");
				ctx.logInt(makeBox<dia_int::PlaceholderError>(base::strConcat(
					"Module lookup failed for language primitive '",
					primitive,
					"' with module path '",
					module_path_str,
					"'"
				)));
				return query::Failed();
			}
			auto module       = module_opt.value();
			auto linked_scope = queryRootScopeOfMainModuleFile(ctx, module);

			// We don't use lookup machinery here
			auto symbols = ctx.query<QuerySymbolsInScope>(linked_scope)->valueOrThrow();
			for (auto sym: symbols)
				if (name(sym) == base::StrID(element_name)) return sym;

			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Can't find symbol for language primitive '", primitive)
			));
			return query::Failed();
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			switch (key.primitive) {
			case LanguagePrimitive::Panic: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::Panic, "core", { "panicking" }, "panic"
				);
			}
			default:
				CORE_PANIC("Unknown language primitive: ", key.primitive);
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLanguagePrimitiveSymID);
}
