#include "lang_primitives.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/scopes/symbol_id.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <unordered_map>

namespace compiler::helios {
	namespace {
		/**
		 * @brief The standard-library location of a language primitive: the package, the module
		 * path within it, the namespace path inside the file, and the name of the element to look up.
		 */
		struct PrimitivePath {
			std::string              package;
			std::vector<std::string> path;
			std::vector<std::string> namespace_path;
			std::string              element;
		};

		const std::unordered_map<LanguagePrimitive, PrimitivePath>& primitivePaths() {
			static const std::unordered_map<LanguagePrimitive, PrimitivePath> paths{
				{ LanguagePrimitive::Panic,
				  { .package = "core", .path = { "panicking" }, .namespace_path = {}, .element = "panic" } },
				{ LanguagePrimitive::String,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "String" } },
				{ LanguagePrimitive::StringifyStr,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyStr" } },
				{ LanguagePrimitive::StringifyChar,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyChar" } },
				{ LanguagePrimitive::StringifyBool,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyBool" } },
				{ LanguagePrimitive::StringifyI64,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyI64" } },
				{ LanguagePrimitive::StringifyU64,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyU64" } },
				{ LanguagePrimitive::StringifyF64,
				  { .package = "core", .path = { "containers" }, .namespace_path = {}, .element = "stringifyF64" } },
			};
			return paths;
		}

		base::Optional<ScopeID> lookupScopeOfNamespacePath(
			query::Context& ctx, ScopeID current, const std::vector<std::string>& namespace_path
		) {
			auto leaves = HInterface::ofScope(current).lookup(ctx, namespace_path.front())->valueOrThrow().leaves;
			if (leaves.size() != 1 || kind(leaves.front()) != SymbolKind::Namespace) return {};
			
			auto next = leaves.front();

			return lookupScopeOfNamespacePath(ctx, next, std::vector<std::string>(namespace_path.begin() + 1, namespace_path.end()));
		}

		/**
		 * @brief Resolves the symbols a language primitive refers to. Returns an empty list when
		 * the module or the element is absent (e.g. a no-std build). Never logs an error.
		 */
		std::vector<SymID> lookupPrimitiveSymbols(query::Context& ctx, LanguagePrimitive primitive) {
			const auto it = primitivePaths().find(primitive);
			if (it == primitivePaths().end()) CORE_PANIC("Unknown language primitive: ", primitive);
			const PrimitivePath& location = it->second;

			std::vector<base::StrID> path_str_ids
				= location.path
			    | std::views::transform([](const std::string& s) { return base::StrID(s); })
			    | std::ranges::to<std::vector>();

			auto module_opt = frontend::getModuleByAbsolutePath(
				ctx, base::StrID(location.package), path_str_ids
			);
			if_opt_none(module_opt) return {};
			auto linked_scope = queryRootScopeOfMainModuleFile(ctx, module_opt.value());

			if (!location.namespace_path.empty()) {
				auto namespace_scope_opt = lookupScopeOfNamespacePath(
					ctx, linked_scope, location.namespace_path
				);
				if_opt_none(namespace_scope_opt) return {};
				linked_scope = namespace_scope_opt.value();				
			}

			return HInterface::ofScope(linked_scope)
			    .lookup(ctx, base::StrID(location.element), { .with_wildcards = false })
			    ->valueOrThrow()
			    .leaves;
		}

		/**
		 * @brief Resolves a language primitive to its single symbol, logging an error and failing
		 * when it is missing or ambiguous.
		 */
		query::QResult<SymID> lookupPrimitiveThrow(query::Context& ctx, LanguagePrimitive primitive) {
			const auto leaves = lookupPrimitiveSymbols(ctx, primitive);
			if (leaves.empty()) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat("Can't find symbol for language primitive '", primitive, "'")
				));
				return query::Failed();
			}
			if (leaves.size() > 1) {
				ctx.logInt(makeBox<dia::PlaceholderError>(base::strConcat(
					"Found multiple primitives with the symbol name '", primitive, "'"
				)));
				return query::Failed();
			}
			return leaves.back();
		}
	}

	bool isLanguagePrimitivePresent(query::Context& ctx, LanguagePrimitive primitive) {
		return not lookupPrimitiveSymbols(ctx, primitive).empty();
	}

	struct IMPLEMENT_QUERY(QueryLanguagePrimitiveSymID, query::QResult<SymID>) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			return lookupPrimitiveThrow(ctx, key.primitive);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLanguagePrimitiveSymID);
}
