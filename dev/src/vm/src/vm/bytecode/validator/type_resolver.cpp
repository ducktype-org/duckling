// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/extend_cpp/variant_match.hpp>

#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_resolver.hpp>

#include <ranges>
#include <utility>
#include <vector>

namespace vm::code {
	base::Optional<CRef<valid_type::ValidType>> findResolved(
		const valid_type::ValidTypeMap& types, const TypeExpr& expr
	) {
		// A type is stored under the spelling of the expression denoting it, so a name and the
		// expression that builds it agree by construction.
		return types.atMaybe(expr.canonicalName());
	}

	CRef<valid_type::ValidType> getResolved(
		const valid_type::ValidTypeMap& types, const TypeExpr& expr
	) {
		return findResolved(types, expr).expect("A validated program only names known types");
	}

	valid_type::ValidTypeID TypeResolver::resolve(const TypeExpr& expr) {
		if (auto type = findResolved(context->getCurrentTypes(), expr))
			return type.value()->getID();

		// A named type is built from its declaration; any other expression describes its type
		// itself.
		base::Optional<CRef<TypeOfData>> declaration;
		if_opt_some(expr.maybeGetKindAs<expr::Named>(), named) {
			declaration = context->findDeclaration(named->name);
			if (!declaration.has_value()) throw UnknownTypeError(opargs::Type(expr));
		}

		const auto id = context->declare(expr);
		built.push_back(id);
		if (declaration.has_value())
			define(id, **declaration);
		else
			defineDerived(id, expr);
		return id;
	}

	void TypeResolver::finalizeBuilt() {
		for (const auto id: built) context->finalize(id);
		built.clear();
	}

	void TypeResolver::defineDerived(valid_type::ValidTypeID id, const TypeExpr& expr) {
		// What the type is built from is resolved before it is fetched for defining, as in
		// `define`.
		variant_match(expr.getKind()) {
			variant_case(expr::Pointer, pointer) {
				const auto pointee = resolve(pointer.pointee);
				context->forDefinition(id).definePointer(pointee);
			}
			variant_case(expr::CPointer, cpointer) {
				const auto pointee
					= cpointer.pointee.map([&](const TypeExpr& inner) { return resolve(inner); });
				context->forDefinition(id).defineCPointer(pointee);
			}
			variant_case_novalue(expr::Named) { CORE_UNREACHABLE(); }
		}
	}

	void TypeResolver::define(valid_type::ValidTypeID id, const TypeOfData& declaration) {
		// Everything a type refers to is resolved before it is fetched for defining, because
		// building another type on the way may move the stored types.
		const auto resolve_all = [&](const auto& exprs) {
			return exprs
			     | std::views::transform([&](const TypeExpr& expr) { return resolve(expr); })
			     | std::ranges::to<std::vector>();
		};
		const auto resolve_fields = [&](const std::vector<Field>& fields) {
			return fields | std::views::transform([&](const Field& field) {
					   return std::make_pair(field.name, resolve(field.type));
				   })
			     | std::ranges::to<std::vector>();
		};
		const auto implementations_of = [](const std::vector<MethodImpl>& implementations) {
			return implementations | std::views::transform([](const MethodImpl& impl) {
					   return std::make_pair(impl.name, impl.function_name);
				   })
			     | std::ranges::to<std::vector>();
		};
		const auto type = [&]() -> valid_type::ValidType& { return context->forDefinition(id); };

		variant_match(declaration) {
			variant_case(PrimitiveType, primitive) {
				type().definePrimitive(Bytes(primitive.size));
			}
			variant_case(FixedSizeTableType, fixed_size_table) {
				const auto inner = resolve(fixed_size_table.inner);
				type().defineFixedSizeTable(inner, fixed_size_table.table_size);
			}
			variant_case(DynamicTableType, dynamic_table) {
				const auto inner = resolve(dynamic_table.inner);
				type().defineDynamicTable(inner);
			}
			variant_case(DataType, data) {
				const auto fields = resolve_fields(data.fields);
				type().defineData(fields, data.packed);
			}
			variant_case(VariantType, variant) {
				const auto alternatives = resolve_all(variant.variant_alternatives);
				type().defineVariant(alternatives);
			}
			variant_case(FunctionType, function) {
				const auto parameters = resolve_all(function.parameters);
				const auto results    = resolve_all(function.result);
				type().defineFunction(parameters, results);
			}
			variant_case(OpaqueType, opaque) { type().defineOpaque(Bytes(opaque.size)); }
			variant_case(ClassType, clazz) {
				const auto fields = resolve_fields(clazz.fields);
				const auto extends
					= clazz.extends.map([&](const TypeExpr& base) { return resolve(base); });
				const auto implements = resolve_all(clazz.implements);
				const auto methods    = resolve_fields(clazz.virtual_methods);
				type().defineClass(
					fields,
					clazz.is_abstract,
					extends,
					implements,
					methods,
					implementations_of(clazz.implementations)
				);
			}
			variant_case(InterfaceType, interface) {
				const auto implements = resolve_all(interface.implements);
				const auto methods    = resolve_fields(interface.virtual_methods);
				type().defineInterface(
					implements, methods, implementations_of(interface.implementations)
				);
			}
		}
	}
}
