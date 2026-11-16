// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lookup.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/selector.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>

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
		pst::Access<pst::Selector> selectorOf(query::Context& ctx, SymID symbol) {
			return usingImportSelector(ctx, getSymRef(symbol)->stmtCast(ctx).value());
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
		using SelectorPath = std::vector<pst::AccessLocked<pst::IdentifierWrapper>>;

		/**
		 * @brief The chain of @p names, looked up one after another starting from @p start.
		 *
		 * Every name has to be found uniquely. When @p start is a symbol, it is the root of the
		 * chain.
		 *
		 * The names are looked up without wildcards: a wildcard cannot be part of what another
		 * wildcard points at, and following them here would make resolving a `using N.*;` ask the
		 * very scope it lives in for `N` again.
		 */
		static query::QResult<LookupPath> resolveChain(
			Context& ctx, std::variant<ScopeID, SymID> start, const SelectorPath& names
		) {
			LookupPath chain;
			if (v_matches(start, SymID)) chain.symbols.push_back(v_get(start, SymID));

			for (const auto& name_locked: names) {
				auto interface = VARIANT_VISIT(
					start,
					VISIT_CASE_VAL(ScopeID, id, HInterface::ofScopeWithParents(id)),
					VISIT_CASE_VAL(SymID, id, HInterface::ofSymbol(ctx, id))
				);
				auto name = name_locked.unlock(ctx);
				UNPACK_QRESULT_MOVE(
					CRef<LookupPath> step =,
					interface.lookupExpectSingle(
						name->getStablePosition(), ctx, name->unwrap(), { .with_wildcards = false }
					)
				);
				base::appendToVector(chain.symbols, step->symbols);
				start = step->leaf();
			}
			return chain;
		}

		/**
		 * @brief The chain of a `using` selector, looked up from the scope the `using` is in.
		 */
		static query::QResult<LookupPath> resolveUsingPath(
			Context& ctx, SymID symbol, pst::Access<pst::Selector> selector
		) {
			return resolveChain(ctx, scope(symbol), selectorPath(selector));
		}

		/**
		 * @brief The chain of an `import` selector.
		 *
		 * The prefix of the selector is a module path, so the lookup continues in the root scope
		 * of that module.
		 */
		static query::QResult<LookupPath> resolveImportPath(
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
			// Nothing left means the selector points at the module itself.
			return resolveChain(
				ctx,
				symOfModule(ctx, current_module),
				SelectorPath(
					path_idents.begin() + base::safeIntConv<int>(names_taken), path_idents.end()
				)
			);
		}

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			const auto symbol_kind = kind(key.symbol);
			CORE_ASSERT(
				symbol_kind == SymbolKind::Using or symbol_kind == SymbolKind::Import, "Invalid call"
			);

			auto selector = selectorOf(ctx, key.symbol);
			UNPACK_QRESULT_MOVE(
				LookupPath chain =,
				symbol_kind == SymbolKind::Using ? resolveUsingPath(ctx, key.symbol, selector)
												 : resolveImportPath(ctx, key.symbol, selector)
			);
			auto pointed = chain.leaf();
			if (not key.keep_whole_chains) chain = stripQualifiers(ctx, std::move(chain), true);

			if (selector->getTailKind() != pst::SelectorTail::Star)
				return UsingImportTarget{ TargetExpr{ std::move(chain) } };

			switch (kind(pointed)) {
			case SymbolKind::Module:
			case SymbolKind::Namespace:
				return UsingImportTarget{ TargetScope{
					.scope = scopeOfNamespaceLike(ctx, pointed),
					.chain = key.keep_whole_chains ? std::move(chain) : LookupPath{},
				} };
			case SymbolKind::Class:
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const:
			case SymbolKind::Field:
				return UsingImportTarget{ WildcardExpr{ std::move(chain) } };
			default:
				ctx.log<dia::NotYetImplementedCodeError>(
					"Wildcard of a symbol that has no interface.", selector->getStablePosition()
				);
				return query::Failed();
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryResolveUsingImport);
}
