// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lookup.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/selector.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/import.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/using.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	ScopeID scopeOfNamespaceLike(query::Context& ctx, SymID symbol) {
		if (kind(symbol) == SymbolKind::Module) {
			auto module_id = getSymRef(symbol)->getData<defgen::Module>()->module_id;
			return queryRootScopeOfMainModuleFile(ctx, module_id);
		}
		CORE_ASSERT(kind(symbol) == SymbolKind::Namespace, "Invalid call");
		return queryBodyCodeScopeFor(ctx, getSymRef(symbol)->stmtCast(ctx).value());
	}

	namespace {
		/**
		 * @brief The selector of the `using`/`import` statement the symbol was made from.
		 * Only the first selector is used, the others are reported as not supported when the
		 * symbol is created.
		 */
		pst::Access<pst::Selector> selectorOf(query::Context& ctx, SymID symbol) {
			auto element = getSymRef(symbol)->maybePstElement().value().unlock(ctx);
			auto selectors
				= element.dynamicCast<pst::Using>().has_value()
			        ? element.dynamicCast<pst::Using>().value()->getSelectors().unlock(ctx)
			        : element.dynamicCast<pst::Import>().value()->getSelectors().unlock(ctx);
			return (*selectors->begin()).unlock(ctx);
		}

		/**
		 * @brief The dotted prefix (`a.b` of `a.b.*` / `a.b as c`) of a single selector.
		 */
		std::vector<pst::AccessLocked<pst::IdentifierWrapper>> selectorPath(
			pst::Access<pst::Selector> selector
		) {
			std::vector<pst::AccessLocked<pst::IdentifierWrapper>> path;
			path.reserve(selector->numberOfNames());
			for (usize i = 0; i < selector->numberOfNames(); i++)
				path.push_back(selector->getNameByIndex(i));
			return path;
		}

		SymID symOfModule(query::Context& ctx, frontend::ModuleID id) {
			return ctx.query<defgen::QueryGeneratedSymbol>({
				.name                  = frontend::moduleName(id),
				.generated_symbol_data = defgen::Module{ .module_id = id },
			});
		}
	}

	query::QResult<bool> usingImportProvidesName(
		query::Context& ctx, SymID symbol, base::StrID name, bool with_wildcards
	) {
		CORE_ASSERT(
			kind(symbol) == SymbolKind::Using or kind(symbol) == SymbolKind::Import, "Invalid call"
		);
		auto selector = selectorOf(ctx, symbol);
		switch (selector->getTailKind()) {
		case pst::SelectorTail::As:
			return selector->getAsName().value().unlock(ctx)->unwrap() == name;
		case pst::SelectorTail::None: {
			if (selector->numberOfNames() == 0) return false;
			auto last_name = selector->getNameByIndex(selector->numberOfNames() - 1);
			return last_name.unlock(ctx)->unwrap() == name;
		}
		case pst::SelectorTail::Star:
			return with_wildcards;
		case pst::SelectorTail::Nested:
			ctx.log<dia::NotYetImplementedCodeError>(
				"Nested selector in usings and imports.", selector->getStablePosition()
			);
			return query::Failed();
		}
		CORE_PANIC("Unhandled selector tail kind");
	}

	struct IMPLEMENT_QUERY(QueryResolveUsingImport, query::QResult<UsingImportTarget>) {
		/**
		 * @brief The symbol a single `using` selector path points at.
		 *
		 * The path is an ordinary chain of names, looked up from the scope the `using` is in.
		 */
		static query::QResult<SymID> resolveUsingPath(
			Context& ctx, SymID symbol, pst::Access<pst::Selector> selector
		) {
			// The path of a `using` is resolved without wildcards: a wildcard cannot be part of
			// what another wildcard points at, and following them here would make resolving a
			// `using N.*;` ask the very scope it lives in for `N` again.
			LookupChainKey chain_key{ .names  = selectorPath(selector),
				                      .start  = scope(symbol),
				                      .params = { .with_wildcards = false } };
			return lastOfChain(ctx, chain_key);
		}

		/**
		 * @brief The symbol a single `import` selector path points at.
		 *
		 * The prefix of the selector is a module path, so the lookup continues in the root scope
		 * of that module.
		 */
		static query::QResult<SymID> resolveImportPath(
			Context& ctx, SymID symbol, pst::Access<pst::Selector> selector
		) {
			auto path_idents = selectorPath(selector);

			// Phase 1: follow the path through the module tree while it names modules.
			usize names_taken    = 0;
			auto  current_module = module(scope(symbol));
			for (; names_taken < path_idents.size(); names_taken++) {
				auto lookup_name = path_idents.at(names_taken).unlock(ctx)->unwrap();
				auto maybe_imported_module
					= frontend::getRelativeModule(ctx, current_module, { lookup_name });
				if (not maybe_imported_module) {
					// The first name has to be a module, the rest may already be its contents.
					if (names_taken == 0) {
						ctx.log<dia::PlaceholderError>(
							"Module not found.", selector->getStablePosition()
						);
						return query::Failed();
					}
					break;
				}
				current_module = maybe_imported_module.value();
			}

			// Phase 2: what is left of the path names items inside the module found above.
			// Nothing left means the selector points at the module itself, which is an empty chain.
			LookupChainKey chain_key{
				.names = std::vector(
					path_idents.begin() + base::safeIntConv<int>(names_taken), path_idents.end()
				),
				.start  = symOfModule(ctx, current_module),
				.params = { .with_wildcards = false },
			};
			return lastOfChain(ctx, chain_key);
		}

		static query::QResult<SymID> lastOfChain(Context& ctx, const LookupChainKey& chain_key) {
			UNPACK_QRESULT_MOVE(auto sym_chain =, lookupChain(ctx, chain_key));
			CORE_ASSERT(
				not sym_chain.empty(),
				"It should be impossible to create chain that doesn't resolve to any symbol."
			);
			return sym_chain.back();
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto symbol_kind = kind(key);
			CORE_ASSERT(
				symbol_kind == SymbolKind::Using or symbol_kind == SymbolKind::Import, "Invalid call"
			);

			auto selector = selectorOf(ctx, key);
			UNPACK_QRESULT(
				SymID pointed =,
				symbol_kind == SymbolKind::Using ? resolveUsingPath(ctx, key, selector)
												 : resolveImportPath(ctx, key, selector)
			);

			if (selector->getTailKind() != pst::SelectorTail::Star)
				return UsingImportTarget{ std::vector{ pointed } };

			if (kind(pointed) != SymbolKind::Module and kind(pointed) != SymbolKind::Namespace) {
				ctx.log<dia::NotYetImplementedCodeError>(
					"Wildcard of a symbol that is not a module or a namespace.",
					selector->getStablePosition()
				);
				return query::Failed();
			}

			return UsingImportTarget{ scopeOfNamespaceLike(ctx, pointed) };
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryResolveUsingImport);
}
