// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "queries.hpp"

#include "abstract_type_impl.hpp"

#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <unordered_set>

namespace compiler::tsh {
	struct IMPLEMENT_QUERY(QueryInterfaceOfClass, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			return ctx.query<compiler::helios::QueryClassSymbolData>(key.value->getSymbol())
			    ->valueOrThrow()
			    .declared_interface;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfClass)

	struct IMPLEMENT_QUERY(QueryInterfaceOfTuple, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			std::vector<InterfaceElement> elements;
			elements.reserve(key.value->getComponents().size());

			auto components = ctx.query<helios::QueryTupleTypeData>(key.value->toAbstractType())
			                      ->valueOrThrow()
			                      .members;
			u32 declaration_order = 0;
			for (const auto& component: components) {
				elements.emplace_back(
					component,
					ctx.query<helios::QueryTypeOfSymbol>(component)->valueOrThrow().getType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					MemberVisibility::Public
				);
				declaration_order++;
			}


			auto interface = TypeInterface(elements);
			return interface;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfTuple)

	struct IMPLEMENT_QUERY(QueryInterfaceOfStaticArray, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			// A static array exposes no fields, only a `length` method.
			const auto length_sym = helios::defgen::lengthMethodForType(ctx, key);

			std::vector<InterfaceElement> elements;
			elements.emplace_back(
				length_sym,
				key,
				0,
				InterfaceElement::InterfaceElementKind::Method,
				MemberVisibility::Public
			);

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfStaticArray)

	struct IMPLEMENT_QUERY(QueryInterfaceOfSlice, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			// A slice exposes the `ptr` and `len` fields, in that order, and a `length` method.
			const auto& fields = ctx.query<compiler::helios::QuerySliceTypeData>(key);

			std::vector<InterfaceElement> elements;
			elements.reserve(3);

			u32 declaration_order = 0;
			for (const compiler::helios::SymID field_sym: { fields->ptr, fields->len }) {
				elements.emplace_back(
					field_sym,
					key,
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					MemberVisibility::Private
				);
				declaration_order++;
			}

			const auto length_sym = helios::defgen::lengthMethodForType(ctx, key);
			elements.emplace_back(
				length_sym,
				key,
				declaration_order,
				InterfaceElement::InterfaceElementKind::Method,
				MemberVisibility::Public
			);

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfSlice)

	struct IMPLEMENT_QUERY(QueryInterfaceOfOptional, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			using helios::LanguagePrimitive;

			const auto value_type = key.getOptionalValueType();

			TypeInterfaceBuilder builder(key);
			for (const auto primitive: { LanguagePrimitive::OptionalValue,
			                             LanguagePrimitive::OptionalValueRef,
			                             LanguagePrimitive::OptionalValueOr,
			                             LanguagePrimitive::OptionalValueOrRef,
			                             LanguagePrimitive::OptionalFull,
			                             LanguagePrimitive::OptionalEmpty,
			                             LanguagePrimitive::OptionalReset }) {
				builder.push(
					helios::bakeLanguagePrimitiveWithTypes(ctx, primitive, { value_type }),
					InterfaceElement::InterfaceElementKind::Method,
					MemberVisibility::Public
				);
			}

			return builder.build();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfOptional)
}
