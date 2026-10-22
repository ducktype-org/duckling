// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "interface.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/lookup/errors.hpp>
#include <helios_private/lookup/lookup.hpp>
#include <helios_private/lookup/lookup_in_type_interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	HInterface HInterface::ofSymbol(query::Context& ctx, SymID symbol) {
		switch (kind(symbol)) {
		case SymbolKind::Module:
			return ofModule(symbol);
		case SymbolKind::Namespace:
			return ofNamespace(symbol);
		case SymbolKind::Using:
			return ofUsing(symbol);
		case SymbolKind::Import:
			return ofImport(symbol);
		case SymbolKind::Class:
			return ofTypeMeta(ctx.query<tsh::QueryClassType>(symbol));
		case SymbolKind::Variable:
		case SymbolKind::Field:
		case SymbolKind::Parameter:
		case SymbolKind::Const:
			return ofTypeInstance(ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow().getType());
		default:
			ctx.log<dia::NotYetImplementedCodeError>(
				base::strConcat("Interface of the ", base::enumToStr(kind(symbol)))
			);
			query::throwFailed();
		}
	}

	CRef<query::QResult<LookupResult>> HInterface::lookup(
		query::Context& ctx, base::StrID name, AdditionalLookupParameters params
	) const {
		variant_match(data) {
			variant_case(ScopeInterface, scope) {
				return ctx.query<QueryLookupInScope>({ scope.scope, name, params.with_wildcards });
			}
			variant_case(ScopeWithParentsInterface, scope) {
				return ctx.query<QueryLookupInScopeAndParents>(
					{ scope.scope, name, params.with_wildcards }
				);
			}
			variant_case(ModuleInterface, symbol) {
				return ctx.query<QueryLookupInNamespaceOrModule>(
					{ symbol.id, name, params.with_wildcards }
				);
			}
			variant_case(NamespaceInterface, symbol) {
				return ctx.query<QueryLookupInNamespaceOrModule>(
					{ symbol.id, name, params.with_wildcards }
				);
			}
			variant_case(UsingInterface, symbol) {
				return ctx.query<QueryLookupInUsingImport>(
					{ symbol.symbol, name, params.with_wildcards }
				);
			}
			variant_case(ImportInterface, symbol) {
				return ctx.query<QueryLookupInUsingImport>(
					{ symbol.symbol, name, params.with_wildcards }
				);
			}
			variant_case(TypeInstanceInterface, type) {
				return ctx.query<QueryLookupInType>(
					{ type.type, name, TypeAccessMode::Instance, params.accessing_scope }
				);
			}
			variant_case(TypeMetaInterface, type) {
				return ctx.query<QueryLookupInType>(
					{ type.type, name, TypeAccessMode::Meta, params.accessing_scope }
				);
			}
			variant_case(CustomInterface, custom) {
				return custom.custom->lookup(ctx, name, params);
			}
		}
		CORE_UNREACHABLE();
	}

	query::QResult<SymbolList> HInterface::lookupExpectUnique(
		const dia::StablePosition  error_position,
		query::Context&            ctx,
		base::StrID                name,
		AdditionalLookupParameters params
	) const {
		UNPACK_QRESULT_CREF(CRef<LookupResult> lookup_result = &, lookup(ctx, name, params));

		auto get_as_single = lookup_result->getAsSingle();

		if (get_as_single.hasFailed()) return query::Failed();

		variant_match(get_as_single.valueOrThrow()) {
			variant_case(SymbolList, symbol_list) { return symbol_list; }
			variant_case(errors::Ambiguity, _) {
				auto msg = makeBox<ShadowedVariableLookupError>(error_position);
				for (auto& leaf: lookup_result->leaves) {
					if_opt_some(getSymRef(leaf)->maybePstElement(), pst_elem) {
						auto decl_pos = pst_elem.unlock(ctx)->getStablePosition();
						msg->addAttachedMessage(makeBox<ShadowingDeclarationNote>(decl_pos));
					}
				}
				ctx.logInt(std::move(msg));
				return query::Failed();
			}
			variant_case(errors::Inaccessible, _) {
				auto msg = makeBox<InaccessibleSymbolLookupError>(error_position);
				for (auto& hidden: lookup_result->inaccessible) {
					if_opt_some(getSymRef(hidden)->maybePstElement(), pst_elem) {
						auto decl_pos = pst_elem.unlock(ctx)->getStablePosition();
						msg->addAttachedMessage(makeBox<InaccessibleDeclarationNote>(decl_pos));
					}
				}
				ctx.logInt(std::move(msg));
				return query::Failed();
			}
			variant_case(errors::SymbolNotFound, _) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat("Symbol '", name, "' not found in lookup"),
					error_position,
					"",
					"symbol lookup here"
				));
				return query::Failed();
			}
			variant_default { CORE_PANIC("Invalid state"); }
		}
		CORE_UNREACHABLE();
	}
}
