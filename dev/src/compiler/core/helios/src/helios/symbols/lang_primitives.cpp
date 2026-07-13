#include "lang_primitives.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryLanguagePrimitiveSymID, query::QResult<SymID>) {
		static query::QResult<SymID> lookupPrimitive(
			query::Context&          ctx,
			LanguagePrimitive        primitive,
			const std::string&       package_name,
			std::vector<std::string> path,
			const std::string&       element_name
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

			auto sym_list = HInterface::ofScope(linked_scope)
			                    .lookup(ctx, base::StrID(element_name), { .with_wildcards = false })
			                    ->valueOrThrow();
			if (sym_list.leaves.size() == 0) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					base::strConcat("Can't find symbol for language primitive '", primitive, "'")
				));
				return query::Failed();
			} else if (sym_list.leaves.size() > 1) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(base::strConcat(
					"Found multiple primitives with the symbol name '", primitive, "'"
				)));
				return query::Failed();
			} else {
				return sym_list.leaves.back();
			}
		}

		static auto provide(query::Context& ctx, QKey key) -> PResult {
			switch (key.primitive) {
			case LanguagePrimitive::Panic: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::Panic, "core", { "panicking" }, "panic"
				);
			}
			case LanguagePrimitive::String: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::String, "core", { "containers" }, "String"
				);
			}
			case LanguagePrimitive::StringifyStr: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyStr, "core", { "containers" }, "stringifyStr"
				);
			}
			case LanguagePrimitive::StringifyChar: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyChar, "core", { "containers" }, "stringifyChar"
				);
			}
			case LanguagePrimitive::StringifyBool: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyBool, "core", { "containers" }, "stringifyBool"
				);
			}
			case LanguagePrimitive::StringifyI64: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyI64, "core", { "containers" }, "stringifyI64"
				);
			}
			case LanguagePrimitive::StringifyU64: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyU64, "core", { "containers" }, "stringifyU64"
				);
			}
			case LanguagePrimitive::StringifyF64: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::StringifyF64, "core", { "containers" }, "stringifyF64"
				);
			}
			case LanguagePrimitive::ConcatStrings: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::ConcatStrings, "core", { "containers" }, "concatStrings"
				);
			}
			case LanguagePrimitive::PrependChar: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::PrependChar, "core", { "containers" }, "prependChar"
				);
			}
			case LanguagePrimitive::AppendChar: {
				return lookupPrimitive(
					ctx, LanguagePrimitive::AppendChar, "core", { "containers" }, "appendChar"
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
