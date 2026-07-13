#include "queries.hpp"

#include "abstract_type_impl.hpp"
#include "helios/symbols/symbol_id.hpp"
#include "helios/tsh/queries/types.hpp"
#include "helios/tsh/type_interface.hpp"
#include "helios/tsh/types.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/definition_generation/list_methods.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <unordered_set>

namespace compiler::tsh {
	struct IMPLEMENT_QUERY(QueryInterfaceOfClass, query::QResult<TypeInterface>) {
		static InterfaceElement::SpecialKind getMethodSpecialKind(Context& ctx, helios::SymID sym) {
#define METHOD_HAS_NO_PARAMS(type) type.getParameterTypes().size() == 1

			auto name = helios::name(sym);
			if (name == base::StrID("toString")) {
				const auto method_type
					= ctx.query<helios::QueryTypeOfSymbol>(sym)->valueOrThrow().getType();
				if (method_type.getKind() == tsh::Kind::Function) {
					const auto fn_type = method_type.as<tsh::FunctionAbstractType>();
					if (METHOD_HAS_NO_PARAMS(fn_type)
					    && fn_type.getResultType() == SymbolType<>::withDefaults(getStringType(ctx)))
						return InterfaceElement::SpecialKind::ToString;
				}
			}
			return InterfaceElement::SpecialKind::None;
		}

		static auto provide(Context& ctx, const QKey key) -> PResult {
			const compiler::helios::SymID symbol = key.value->getSymbol();

			const auto& class_data
				= ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow();

			std::vector<InterfaceElement> elements;
			elements.reserve(class_data.members.size() + class_data.methods.size());

			u32 declaration_order = 0;
			for (const compiler::helios::SymID field_sym: class_data.members) {
				elements.push_back(InterfaceElement(
					field_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					{}
				));
				declaration_order++;
			}

			for (const compiler::helios::SymID method_sym: class_data.methods) {
				elements.push_back(InterfaceElement(
					method_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Method,
					{},
					getMethodSpecialKind(ctx, method_sym)
				));
				declaration_order++;
			}

			for (const compiler::helios::SymID ctor_sym: class_data.constructors) {
				// For now we just handle copy constructors.
				if (!compiler::helios::defgen::isUserDefinedCopyConstructor(ctx, ctor_sym))
					continue;
				elements.push_back(InterfaceElement(
					ctor_sym,
					key.value->toAbstractType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Method,
					{}
				));
				declaration_order++;
			}

			return TypeInterface(elements);
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
			u32  declaration_order = 0;
			for (const auto& component: components) {
				elements.emplace_back(
					component,
					ctx.query<helios::QueryTypeOfSymbol>(component)->valueOrThrow().getType(),
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					ClassMemberVisibility::Public
				);
				declaration_order++;
			}


			auto interface = TypeInterface(elements);
			return interface;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfTuple)

	struct IMPLEMENT_QUERY(QueryInterfaceOfDynamicArray, query::QResult<TypeInterface>) {
		static auto provide(Context& ctx, const QKey key) -> PResult {
			// A dynamic array exposes the `list` struct fields (ptr, len,
			// off_start_reserved, off_end_reserved) and a `length`, `push` and `pop` method.
			const auto& fields = ctx.query<compiler::helios::QueryDynamicArrayTypeData>(key);

			std::vector<InterfaceElement> elements;
			elements.reserve(7);

			u32 declaration_order = 0;
			for (const compiler::helios::SymID field_sym:
			     { fields->ptr, fields->len, fields->off_start_reserved, fields->off_end_reserved }) {
				elements.emplace_back(
					field_sym,
					key,
					declaration_order,
					InterfaceElement::InterfaceElementKind::Field,
					ClassMemberVisibility::Private
				);
				declaration_order++;
			}

			const auto length_sym = helios::defgen::lengthMethodForType(ctx, key);
			elements.emplace_back(
				length_sym,
				key,
				declaration_order,
				InterfaceElement::InterfaceElementKind::Method,
				ClassMemberVisibility::Public
			);
			declaration_order++;

			const auto push_sym = helios::defgen::pushMethodForType(ctx, key);
			elements.emplace_back(
				push_sym,
				key,
				declaration_order,
				InterfaceElement::InterfaceElementKind::Method,
				ClassMemberVisibility::Public
			);
			declaration_order++;

			const auto pop_sym = helios::defgen::popMethodForType(ctx, key);
			elements.emplace_back(
				pop_sym,
				key,
				declaration_order,
				InterfaceElement::InterfaceElementKind::Method,
				ClassMemberVisibility::Public
			);
			declaration_order++;

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfDynamicArray)

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
				ClassMemberVisibility::Public
			);

			return TypeInterface(elements);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryInterfaceOfStaticArray)
}
