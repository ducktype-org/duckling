#include "lang_primitives.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/templates/templates.hpp>

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
			std::vector<std::string> namespaces;
			std::string              element;
		};

		const std::unordered_map<LanguagePrimitive, PrimitivePath>& primitivePaths() {
			static const std::unordered_map<LanguagePrimitive, PrimitivePath> paths{
				{ LanguagePrimitive::Panic,
				  { .package    = "core",
				    .path       = { "panicking" },
				    .namespaces = {},
				    .element    = "panic" } },
        { LanguagePrimitive::PowInt,
				  { .package = "std", .path = { "math" }, .element = "powi" } },
				{ LanguagePrimitive::PowF32,
				  { .package = "std", .path = { "math" }, .element = "powf" } },
				{ LanguagePrimitive::PowF64,
				  { .package = "std", .path = { "math" }, .element = "pow" } },
				{ LanguagePrimitive::String,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = {},
				    .element    = "String" } },
				{ LanguagePrimitive::StringifyStr,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyStr" } },
				{ LanguagePrimitive::StringifyChar,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyChar" } },
				{ LanguagePrimitive::StringifyBool,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyBool" } },
				{ LanguagePrimitive::StringifyI64,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyI64" } },
				{ LanguagePrimitive::StringifyU64,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyU64" } },
				{ LanguagePrimitive::StringifyF64,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyF64" } },
				{ LanguagePrimitive::StringifyPtr,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyPtr" } },
				{ LanguagePrimitive::StringifyManyPtr,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyManyPtr" } },
				{ LanguagePrimitive::StringifyCPtr,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyCPtr" } },
				{ LanguagePrimitive::StringifySlice,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifySlice" } },
				{ LanguagePrimitive::StringifyStaticArray,
				  { .package    = "core",
				    .path       = { "containers" },
				    .namespaces = { "stringification" },
				    .element    = "stringifyStaticArray" } },
			};
			return paths;
		}

		base::MBox<HInterface> lookupInterfaceOfNamespacePath(
			query::Context&                 ctx,
			base::Box<HInterface>           current,
			const std::vector<std::string>& namespaces
		) {
			if (namespaces.empty()) return std::move(current);

			auto lookup_result
				= current->lookup(ctx, base::StrID{ namespaces.front() })->valueOrThrow();
			if (!lookup_result.isSingle()) return nullptr;

			auto next = std::get<SymbolList>(lookup_result.getAsSingle().valueOrPanic()).back();

			return lookupInterfaceOfNamespacePath(
				ctx,
				base::makeBox<HInterface>(HInterface::ofSymbol(next)),
				std::vector<std::string>(namespaces.begin() + 1, namespaces.end())
			);
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

			auto namespace_interface
				= lookupInterfaceOfNamespacePath(
					  ctx,
					  base::makeBox<HInterface>(HInterface::ofScope(linked_scope)),
					  location.namespaces
				)
			          .toOptBox();
			if_opt_none(namespace_interface) return {};

			return namespace_interface.value()
			    ->lookup(ctx, base::StrID(location.element), { .with_wildcards = false })
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

	SymID bakeLanguagePrimitive(
		query::Context&                    ctx,
		LanguagePrimitive                  primitive,
		std::vector<ctv::CompileTimeValue> ctv_arguments
	) {
		const SymID template_sym
			= ctx.query<QueryLanguagePrimitiveSymID>({ primitive })->valueOrThrow();

		const templates::TemplateBakeKey key{
			.template_sym_id    = template_sym,
			.template_arguments = std::move(ctv_arguments),
		};
		return ctx.query<templates::QueryBakeTemplateSymID>(key).valueOrThrow();
	}

	SymID bakeLanguagePrimitiveWithTypes(
		query::Context&                ctx,
		LanguagePrimitive              primitive,
		std::vector<tsh::SymbolType<>> type_arguments
	) {
		std::vector ctv_args
			= type_arguments
		    | std::views::transform([](auto& type) { return ctv::CompileTimeValue(type); })
		    | std::ranges::to<std::vector>();
		return bakeLanguagePrimitive(ctx, primitive, std::move(ctv_args));
	}
}
