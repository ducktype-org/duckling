#include "type_builder.hpp"

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_utils.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <ranges>

namespace {
	using namespace vm::code::detail;

	/**
	 * @brief Recursively builds a vtable layout for an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType, TypeOfDataConcept ErrorContextType>
	void buildVTableRecursive(
		const InheritableType&                   inh,
		const ErrorContextType&                  error_context_inh,
		base::HashMap<base::StrID, base::StrID>& vtable,
		vm::TypeMetadata&                        metadata,
		const TypeContext&                       ctx
	) {
		for (const auto& impl: inh.implementations) vtable.put(impl.name, impl.type);
		for (const auto& interface_name: inh.implements) {
			const auto& interface = [&]() -> const InterfaceType& {
				const auto& tod = *ctx.getCurrentTypes().at(interface_name);
				return std::get<InterfaceType>(tod);
			}();
			buildVTableRecursive(interface, error_context_inh, vtable, metadata, ctx);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class = [&]() -> const ClassType& {
					const auto& tod = *ctx.getCurrentTypes().at(*inh.extends);
					return std::get<ClassType>(tod);
				}();
				buildVTableRecursive(super_class, error_context_inh, vtable, metadata, ctx);
			}
		}
	}

	/**
	 * @brief Builds a vector of fields (FieldVector) for an InheritableType. Collects all
	 * fields from superclasses.
	 */
	template<InheritableTypeConcept InheritableType>
	FieldVector buildFieldVector(
		const InheritableType& inh, vm::TypeMetadata& metadata, const TypeContext& ctx
	) {
		auto to_low_type = [&](const TypeOfData& tod) {
			return metadata.at(VISIT(tod, type, return type.name));
		};
		FieldVector fields{ { base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) } };

		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			std::function<void(const vm::code::ClassType&)> collect_class_fields_recursive
				= [&](const vm::code::ClassType& clazz) {
					  if (clazz.extends) {
						  const auto& super_class = [&]() -> const ClassType& {
							  const auto& tod = *ctx.getCurrentTypes().at(*clazz.extends);
							  return std::get<ClassType>(tod);
						  }();
						  collect_class_fields_recursive(super_class);
					  }

					  for (const Field& field_code: clazz.fields)
						  fields.emplace_back(field_code.name, metadata.at(field_code.type));
				  };
			collect_class_fields_recursive(inh);
		}
		return fields;
	}

	/**
	 * @brief Builds inheritance metadata for a given inheritable. Builds a field vector and a
	 * vtable.
	 */
	template<InheritableTypeConcept InheritableType>
	vm::InheritanceMetadata buildInheritanceMetadata(
		const InheritableType& inh, vm::TypeMetadata& metadata, const TypeContext& ctx
	) {
		vm::TypeCRef tp    = metadata.at(inh.name);
		auto get_type_cref = [&](base::StrID name) -> vm::TypeCRef { return metadata.at(name); };
		auto implements    = inh.implements | std::views::transform(get_type_cref)
		                | std::ranges::to<std::vector>();

		base::HashMap<base::StrID, vm::TypeCRef> virtual_methods;
		for (auto& method: inh.virtual_methods)
			virtual_methods.put(method.name, get_type_cref(method.type));

		base::HashMap<base::StrID, base::StrID> vtable;
		buildVTableRecursive(inh, inh, vtable, metadata, ctx);

		vm::InheritanceMetadata::Kind kind;
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			kind = vm::InheritanceMetadata::Class{
				.is_abstract = inh.is_abstract,
				.extends     = inh.extends.map(get_type_cref),
			};
		} else {
			kind = vm::InheritanceMetadata::Interface{};
		}

		return {
			tp, kind, std::move(implements), std::move(virtual_methods), std::move(vtable),
		};
	}

	/**
	 * @brief Declare types from a list in the given type_metadata.
	 */
	void declareTypes(Ref<vm::TypeMetadata> type_metadata, const std::vector<TypeOfData>& types) {
		for (const auto& type: types) type_metadata->addType(vm::Type::declareType(typeName(type)));
	}

	/**
	 * @brief Define types from the list in the given type_metadata.
	 */
	void defineTypes(
		Ref<vm::TypeMetadata>          type_metadata,
		const TypeContext&             ctx,
		const std::vector<TypeOfData>& types
	) {
		for (const auto& type: types) {
			variant_match(type) {
				variant_case(PrimitiveType, data) {
					type_metadata->at(data.name)->definePrimitive(data.size);
				}
				variant_case(PointerType, data) {
					type_metadata->at(data.name)->definePointer(type_metadata->at(data.inner));
				}
				variant_case(FixedSizeTableType, data) {
					type_metadata->at(data.name)->defineFixedSizeTable(
						type_metadata->at(data.inner), data.table_size
					);
				}
				variant_case(DynamicTableType, data) {
					type_metadata->at(data.name)->defineDynamicTable(type_metadata->at(data.inner));
				}
				variant_case(DataType, data) {
					FieldVector fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, type_metadata->at(field.type));
					type_metadata->at(data.name)->defineData(fields, {});
				}
				variant_case(VariantType, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.variant_alternatives.size());
					for (auto& variant: data.variant_alternatives)
						variants.emplace_back(type_metadata->at(variant));
					type_metadata->at(data.name)->defineVariant(variants);
				}
				variant_case(FunctionType, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters)
						parameters.emplace_back(type_metadata->at(param));
					type_metadata->at(data.name)->defineFunction(
						parameters, type_metadata->at(data.result)
					);
				}
				variant_case(OpaqueType, opaque) {
					type_metadata->at(opaque.name)->defineOpaque(opaque.size);
				}
				variant_case(ClassType, clazz) {
					vm::TypeRef             tp     = type_metadata->at(clazz.name);
					FieldVector             fields = buildFieldVector(clazz, *type_metadata, ctx);
					vm::InheritanceMetadata inh_metadata
						= buildInheritanceMetadata(clazz, *type_metadata, ctx);
					tp->defineData(fields, std::move(inh_metadata));
				}
				variant_case(InterfaceType, interface) {
					vm::TypeRef tp     = type_metadata->at(interface.name);
					FieldVector fields = buildFieldVector(interface, *type_metadata, ctx);
					vm::InheritanceMetadata inh_metadata
						= buildInheritanceMetadata(interface, *type_metadata, ctx);
					tp->defineData(fields, std::move(inh_metadata));
				}
				variant_default {
					CORE_PANIC("Unhandled type during type building: ", typeToString(type));
				}
			}
		}
	}
}

Box<vm::TypeMetadata> vm::code::detail::buildTypeMetadata(const TypeContext& ctx) {
	Box<vm::TypeMetadata> type_metadata = makeBox<vm::TypeMetadata>();

	auto types = ctx.getCurrentTypes() | std::ranges::to<std::vector<TypeOfData>>();

	// Declare all types first.
	declareTypes(type_metadata.refMut(), types);

	// Well-define every type.
	defineTypes(type_metadata.refMut(), ctx, types);

	type_metadata->finalize();
	return type_metadata;
}

void vm::code::detail::rebuildTypeMetadata(
	Ref<vm::TypeMetadata> type_metadata, const TypeContext& new_ctx
) {
	auto new_types = new_ctx.getCurrentTypes() | std::views::drop(type_metadata->size())
	               | std::ranges::to<std::vector<TypeOfData>>();
	
	// Reopen type metadata for addition;
	type_metadata->unfinalize();

	// Declare new types.
	declareTypes(type_metadata, new_types);
	// Well define new types.
	defineTypes(type_metadata, new_ctx, new_types);

	type_metadata->finalize();
}
