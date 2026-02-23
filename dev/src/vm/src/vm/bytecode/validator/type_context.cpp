#include "type_context.hpp"

#include <string_id/string_id.hpp>

#include "vm/bytecode/validator/type_validator.hpp"
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type.hpp>

using namespace vm::code;

namespace {
	void defineTypeFromData(const TypeMap& types, type::Type& tp, const TypeOfData& type_of_data) {
		variant_match(type_of_data) {
			variant_case(vm::code::PrimitiveType, primitive) {
				tp.definePrimitive(Bytes(primitive.size));
			}
			variant_case(vm::code::PointerType, pointer) {
				tp.definePointer(types.at(pointer.inner)->getId());
			}
			variant_case(vm::code::FixedSizeTableType, fixed_size_table) {
				tp.defineFixedSizeTable(
					types.at(fixed_size_table.inner)->getId(), fixed_size_table.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, dynamic_table) {
				tp.defineDynamicTable(types.at(dynamic_table.inner)->getId());
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, type::TypeID>> field_definitions;
				field_definitions.reserve(data.fields.size());
				for (const auto& field: data.fields)
					field_definitions.emplace_back(field.name, types.at(field.type)->getId());
				tp.defineData(field_definitions);
			}
			variant_case(vm::code::VariantType, variant) {
				std::vector<vm::code::type::TypeID> variant_types;
				variant_types.reserve(variant.variant_alternatives.size());
				for (const auto& variant_type: variant.variant_alternatives)
					variant_types.push_back(types.at(variant_type)->getId());
				tp.defineVariant(variant_types);
			}
			variant_case(vm::code::FunctionType, function) {
				std::vector<vm::code::type::TypeID> parameter_types;
				parameter_types.reserve(function.parameters.size());
				for (const auto& param: function.parameters)
					parameter_types.push_back(types.at(param)->getId());
				tp.defineFunction(parameter_types, types.at(function.result)->getId());
			}
			variant_case(vm::code::OpaqueType, opaque) { tp.defineOpaque(Bytes(opaque.size)); }
			variant_case(vm::code::ClassType, clazz) {
				std::vector<std::pair<base::StrID, vm::code::type::TypeID>> fields_definitions;
				fields_definitions.reserve(clazz.fields.size());
				for (const auto& field: clazz.fields)
					fields_definitions.emplace_back(field.name, types.at(field.type)->getId());
				tp.defineClass(
					fields_definitions,
					clazz.is_abstract,
					clazz.extends
						? base::Optional<vm::code::type::TypeID>(types.at(*clazz.extends)->getId())
						: base::Optional<vm::code::type::TypeID>(),
					clazz.implements | std::views::transform([&](const auto& i) {
						return types.at(i)->getId();
					}) | std::ranges::to<std::vector>(),
					clazz.virtual_methods | std::views::transform([&](const auto& method) {
						return std::make_pair(method.name, types.at(method.type)->getId());
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
						return types.at(i)->getId();
					}) | std::ranges::to<std::vector>(),
					interface.virtual_methods | std::views::transform([&](const auto& method) {
						return std::make_pair(method.name, types.at(method.type)->getId());
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

void vm::code::TypeContext::insertAndValidate(
	const std::vector<TypeOfData>&                   new_types,
	const base::HashMap<base::StrID, FuncSignature>& function_signatures
) {
	// Simple check for duplicates and forward declarations.
	for (const auto& type: new_types) {
		const auto name = typeName(type);
		if (pod_types.contains(name)) {
			if (type != *pod_types.at(name)) throw DuplicatedTypeError(type, *pod_types.at(name));
		} else {
			pod_types.insert(type, name);
			types.insert(vm::code::type::Type::declareType(name, types.size()), name);
		}
	}

	// Validate
	const std::vector<usize> new_types_id
		= new_types
	    | std::views::transform([&](const auto& type) { return types.at(typeName(type))->getId(); })
	    | std::ranges::to<std::vector>();
	detail::validateTypes(pod_types, new_types_id, function_signatures);

	// Define
	for (const auto& type: new_types) defineTypeFromData(types, *types.at(typeName(type)), type);

	// Finalize
	for (const auto& type: new_types) {
		auto& tp = *types.at(typeName(type));
		tp.finalize(types);
	}
}

const TypeMap& TypeContext::getCurrentTypes() const { return types; }

const vm::ObjIdNameMap<TypeOfData>& vm::code::TypeContext::getPodTypes() const { return pod_types; }
