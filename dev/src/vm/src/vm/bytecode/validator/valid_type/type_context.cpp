#include "type_context.hpp"

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>

using namespace vm::code;

namespace {
	void defineTypeFromData(
		const valid_type::ValidTypeMap& types,
		valid_type::ValidType&          tp,
		const TypeOfData&               type_of_data
	) {
		variant_match(type_of_data) {
			variant_case(vm::code::PrimitiveType, primitive) {
				tp.definePrimitive(Bytes(primitive.size));
			}
			variant_case(vm::code::PointerType, pointer) {
				tp.definePointer(types.at(pointer.inner)->getID());
			}
			variant_case(vm::code::CPointerType, cpointer) {
				tp.defineCPointer(cpointer.inner.map([&](const base::StrID& inner) {
					return types.at(inner)->getID();
				}));
			}
			variant_case(vm::code::FixedSizeTableType, fixed_size_table) {
				tp.defineFixedSizeTable(
					types.at(fixed_size_table.inner)->getID(), fixed_size_table.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, dynamic_table) {
				tp.defineDynamicTable(types.at(dynamic_table.inner)->getID());
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, valid_type::ValidTypeID>> field_definitions;
				field_definitions.reserve(data.fields.size());
				for (const auto& field: data.fields)
					field_definitions.emplace_back(field.name, types.at(field.type)->getID());
				tp.defineData(field_definitions, data.packed);
			}
			variant_case(vm::code::VariantType, variant) {
				std::vector<vm::code::valid_type::ValidTypeID> variant_types;
				variant_types.reserve(variant.variant_alternatives.size());
				for (const auto& variant_type: variant.variant_alternatives)
					variant_types.push_back(types.at(variant_type)->getID());
				tp.defineVariant(variant_types);
			}
			variant_case(vm::code::FunctionType, function) {
				std::vector<vm::code::valid_type::ValidTypeID> parameter_types, return_types;
				parameter_types.reserve(function.parameters.size());
				return_types.reserve(function.result.size());
				for (const auto& param: function.parameters)
					parameter_types.push_back(types.at(param)->getID());
				for (const auto& reslt: function.result)
					return_types.push_back(types.at(reslt)->getID());
				tp.defineFunction(parameter_types, return_types);
			}
			variant_case(vm::code::OpaqueType, opaque) { tp.defineOpaque(Bytes(opaque.size)); }
			variant_case(vm::code::ClassType, clazz) {
				std::vector<std::pair<base::StrID, vm::code::valid_type::ValidTypeID>>
					fields_definitions;
				fields_definitions.reserve(clazz.fields.size());
				for (const auto& field: clazz.fields)
					fields_definitions.emplace_back(field.name, types.at(field.type)->getID());
				tp.defineClass(
					fields_definitions,
					clazz.is_abstract,
					clazz.extends ? base::Optional<vm::code::valid_type::ValidTypeID>(
										types.at(*clazz.extends)->getID()
									)
								  : base::Optional<vm::code::valid_type::ValidTypeID>(),
					clazz.implements | std::views::transform([&](const auto& i) {
						return types.at(i)->getID();
					}) | std::ranges::to<std::vector>(),
					clazz.virtual_methods | std::views::transform([&](const auto& method) {
						return std::make_pair(method.name, types.at(method.type)->getID());
					}) | std::ranges::to<std::vector>(),
					clazz.implementations | std::views::transform([&](const auto& impl) {
						return std::make_pair(
							impl.name, impl.type
						);  // This field is called `type`, though it's actually a name of a function.
					}) | std::ranges::to<std::vector>()
				);
			}
			variant_case(vm::code::InterfaceType, interface) {
				tp.defineInterface(
					interface.implements | std::views::transform([&](const auto& i) {
						return types.at(i)->getID();
					}) | std::ranges::to<std::vector>(),
					interface.virtual_methods | std::views::transform([&](const auto& method) {
						return std::make_pair(method.name, types.at(method.type)->getID());
					}) | std::ranges::to<std::vector>(),
					interface.implementations | std::views::transform([&](const auto& impl) {
						return std::make_pair(
							impl.name, impl.type
						);  // This field is called `type`, though it's actually a name of a function.
					}) | std::ranges::to<std::vector>()
				);
			}
			variant_default {
				CORE_PANIC("Unhandled type during type building: ", typeToString(type_of_data));
			}
		}
	}
}

void TypeContext::insertAndValidate(
	const std::vector<TypeOfData>&                   new_types,
	const base::HashMap<base::StrID, FuncSignature>& function_signatures
) {
	// Simple check for duplicates and forward declarations.
	std::vector<CRef<TypeOfData>> really_new_types;
	for (const auto& type: new_types) {
		const auto name = typeName(type);
		if (tod_types.contains(name)) {
			if (type != *tod_types.at(name)) throw DuplicatedTypeError(type, *tod_types.at(name));
		} else {
			tod_types.insert(type, name);
			types.insert(
				vm::code::valid_type::ValidType::declareType(
					name, valid_type::ValidTypeID(types.size())
				),
				name
			);
			really_new_types.emplace_back(&type);
		}
	}

	// Validate
	const std::vector<usize> new_types_id = really_new_types
	                                      | std::views::transform([&](const auto& type) {
												return tod_types.idOf(typeName(*type)).value();
											})
	                                      | std::ranges::to<std::vector>();
	detail::validateTypes(tod_types, new_types_id, function_signatures);

	// Define
	for (const auto& type: really_new_types)
		defineTypeFromData(types, *types.at(typeName(*type)), *type);

	// Finalize
	for (const auto& type: really_new_types) {
		auto& tp = *types.at(typeName(*type));
		tp.finalize(types);
	}
}

const valid_type::ValidTypeMap& TypeContext::getCurrentTypes() const { return types; }

const vm::ObjIdNameMap<TypeOfData>& vm::code::TypeContext::getTodTypes() const { return tod_types; }
