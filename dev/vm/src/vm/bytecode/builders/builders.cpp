#include "builders.hpp"

#include "base/maps.hpp"
#include "base/string_id.hpp"
#include <base/exceptions.hpp>
#include <base/macros/for_each.hpp>
#include <base/optional.hpp>
#include <base/ref.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include "vm/core/process/type_metadata/definitions.hpp"
#include <vm/bytecode/builders/errors.hpp>
#include <vm/bytecode/builders/function_validator.hpp>
#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>

#include <algorithm>
#include <ranges>

using namespace vm::code::builders;

FunctionBuilder::FunctionBuilder(
	vm::code::Identifier name, const GlobalDataMap& globals, const TypeContext& types
):
	  name(name),
	  type_context(types),
	  globals(globals) {}

vm::code::Function FunctionBuilder::build() const {
	FunctionValidator validator(name, type_context, globals, instructions);
	validator.validate();
	Function function;
	function.body = validator.extractReachableCode();
	function.name = name;
	return function;
}

void FunctionBuilder::addInstruction(const Instruction& instruction) {
	instructions.push_back(instruction);
}

void FunctionBuilder::addInstruction(const InstructionBuilder& instruction) {
	for (auto&& instr: instruction.build()) this->addInstruction(instr);
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& vm::code::builders::TypeContextBuilder::getTypes(
) const {
	return types;
}

void TypeContextBuilder::validateType(const TypeOfData& type) const {
	auto get_type = [&](base::StrID name) {
		return types.atMaybe(name).expect<UnknownSubtypeError>(type, name);
	};
	// auto get_type = [&]<typename T>(base::StrID name, auto error_factory) -> T {
	// 	auto type_variant = get_type_variant(name);
	// 	if (!std::holds_alternative<T>(type_variant)) error_factory();
	// 	return std::get<T>(type_variant);
	// };

	// Validates if all fields in a specified view match. Throws a specified error on failure.
	auto validate_range_match
		= [&](const auto& actual_view, const auto& expected_view, auto error_factory) {
			  const auto actual_size   = std::ranges::size(actual_view);
			  const auto expected_size = std::ranges::size(expected_view);

			  if (actual_size < expected_size) throw error_factory();

			  for (auto [actual, expected]: std::views::zip(actual_view, expected_view))
				  if (actual != expected) throw error_factory();
		  };

	// Validates if implements contain interfaces.
	auto validate_implements = [&](const std::vector<base::StrID>& implements) {
		for (const auto& impl: implements) {
			auto impl_type = get_type(impl);
			if (!std::holds_alternative<InterfaceType>(*impl_type) || *impl_type == type)
				throw InvalidImplementsError(type);
		}
	};

	// Validates if all superclass fields exist in the subclass.
	auto validate_fields = [&](const ClassType& clazz, const ClassType& superclass) {
		validate_range_match(clazz.fields, superclass.fields, [&]() {
			return MissingAncestorFieldError(type);
		});
	};

	// Validates if all superclass/interface vmethods exist in the subclass.
	auto validate_virtual_methods = [&](const ClassType& clazz, const ClassType& superclass) {
		auto error_factory = [&]() { return MissingAncestorVirtualMethodError(type); };

		u64 start_index = 0;
		validate_range_match(clazz.virtual_methods, superclass.virtual_methods, error_factory);
		start_index += superclass.virtual_methods.size();
		for (const auto& interface_name: clazz.implements) {
			// TODO: Validate valid interfaces type here.
			const auto& interface      = std::get<InterfaceType>(*get_type(interface_name));
			auto        vmethods_range = clazz.virtual_methods | std::views::drop(start_index)
			                    | std::views::take(interface.virtual_methods.size());
			validate_range_match(vmethods_range, interface.virtual_methods, error_factory);
			start_index += interface.virtual_methods.size();
		}
	};

	auto validate_implementations = [&](const ClassType& clazz) {
		for (const auto& impl: clazz.vmethods_implementations) {
			// TODO: Potentially change that to a map for faster lookup.
			auto vmethod = std::ranges::find_if(clazz.virtual_methods, [&](const auto& vmethod) {
				return vmethod == impl;
			});
			if (vmethod == clazz.virtual_methods.end())
				throw InvalidVirtualMethodImplementationError(type, impl.name);


			auto impl_type    = *get_type(impl.type);
			auto vmethod_type = *get_type(vmethod->type);
			if (!std::holds_alternative<FunctionType>(impl_type))
				throw TypeIsNotFunctionalError(impl.type);
			if (!std::holds_alternative<FunctionType>(vmethod_type))
				throw TypeIsNotFunctionalError(vmethod->type);

			auto impl_ftype    = std::get<FunctionType>(impl_type);
			auto vmethod_ftype = std::get<FunctionType>(vmethod_type);

			auto validate_method_signatures
				= [&](const FunctionType& vmethod, const FunctionType& impl) {
					  if (vmethod.result != impl.result) throw MethodTypeError(impl);
					  if (vmethod.parameters.size() != impl.parameters.size())
						  throw MethodTypeError(impl);

					  // First argument should always be a class pointer (this*).
				      // TODO: Refactor this code? We use those get<T>'s a lot.
					  auto first_param_type = *get_type(impl.parameters[0]);
					  if (!std::holds_alternative<PointerType>(first_param_type))
						  throw MethodFirstArgumentError(first_param_type);
					  auto first_param_ptr = std::get<PointerType>(first_param_type);

					  // TODO: Make that work for interfaces as well.
				      // using Type = decltype(clazz);
					  if (first_param_ptr.inner != clazz.name)
						  throw MethodFirstArgumentError(first_param_type);

					  for (u64 i = 1; impl.parameters.size(); i++)
						  if (impl.parameters[i] != vmethod.parameters[i])
							  throw MethodTypeError(first_param_type);
				  };
			validate_method_signatures(vmethod_ftype, impl_ftype);
		}
	};

	auto validate_all_methods_implemented = [&](const ClassType& clazz) {
		if (clazz.is_abstract) return;

		// Collect all methods on the inheritance path and verify they are implemented.
		base::HashMap<base::StrID, base::StrID> required_methods;
		for (const auto& vmethod: clazz.virtual_methods) required_methods.put(vmethod.name);

		std::function<void(const ClassType&)> collect_implementations
			= [&](const ClassType& clazz) {
				  // Remove those implemented by this class.
				  for (const auto& impl: clazz.vmethods_implementations)
					  required_methods.erase(impl.name);

				  // Remove those implemented by interfaces.
				  for (const auto& iface: clazz.implements) {
					  //@note: We are guarenteed an InterfaceType is contained. This was checked before.
					  const auto& iface_type = std::get<InterfaceType>(*get_type(iface));
					  for (const auto& impl: iface_type.vmethods_implementations)
						  required_methods.erase(impl.name);
				  }

				  if (clazz.extends) {
					  const auto& super_type = *get_type(*clazz.extends);
					  if (std::holds_alternative<ClassType>(super_type))
						  collect_implementations(std::get<ClassType>(super_type));
				  }
			  };
		if (!required_methods.empty()) {

			throw UnimplementedVirtualMethodError(type, required_methods.begin()->first);
		}
	};


	variant_match(type) {
		variant_case(InterfaceType, interface) {
			validate_implements(interface.implements);
			// TODO: Validate virtual methods. Currently only works for classes. Should work for
			// both. validate_implementations(clazz);
		}
		variant_case(ClassType, clazz) {
			validate_implements(clazz.implements);

			if_opt_some(clazz.extends, extends) {
				auto super_type = get_type(extends);
				if (!std::holds_alternative<ClassType>(*super_type) || *super_type == type)
					throw InvalidExtends(type);
				const auto& superclass = std::get<ClassType>(*super_type);
				// TODO: Merge those comments into one.
				// @note: The order of field declarations in this check is important.
				validate_fields(clazz, superclass);
				// @note: Order of functions is important. We first declare vmethods from the
				// superclass, then virtual methods from interfaces in the order they are declared
				// in their types.
				validate_virtual_methods(clazz, superclass);
				validate_implementations(clazz);
				validate_all_methods_implemented(clazz);


				// TODO: Check if we don't try to call methods without the being in class -> this
				// should appear in function_validator.
				if (!clazz.is_abstract) {
					// TODO: Verify all virtual methods on the superclass path are implemented.
				}
			}
		}
	}
}

void TypeContextBuilder::validateTypes() const {
	for (const auto& type: types) validateType(type);

	// Check for cycles in hierarchy.
	enum Status { Waiting, Visited, Done };

	base::HashMap<base::StrID, Status> status;
	for (const auto& type: types) status.put(typeName(type), Waiting);

	// explicit object parameter lambdas don't seem to work with class members, hence the
	// reference
	auto& types_ref = types;
	auto  helper    = [&](this auto self, const auto& type) {
        auto name = typeName(type);
        if (status[name] == Visited)
            throw CycleInHierarchyError(type);
        else if (status[name] == Done)
            return;

        status[name] = Visited;
        variant_match(type) {
            variant_case(ClassType, clazz) {
                if_opt_some(clazz.extends, superclass) self(*types_ref.at(superclass));
                for (auto iface: clazz.implements) self(*types_ref.at(iface));
            }
            variant_case(InterfaceType, interface) {
                for (auto iface: interface.implements) self(*types_ref.at(iface));
            }
        }
        status[name] = Done;
	};

	for (const auto& type: types) helper(type);
}

TypeContext TypeContextBuilder::build() const {
	validateTypes();
	TypeContext tctx;
	tctx.types = types;
	for (const auto& type: types)
		tctx.metadata->addType(Type::declareType(VISIT(type, tp, return tp.name)));
	auto to_low_type
		= [&](const TypeOfData& tod) { return tctx.metadata->at(VISIT(tod, tp, return tp.name)); };
	for (const auto& type: tctx.types) {
		variant_match(type) {
			variant_case(vm::code::PrimitiveType, data) {
				tctx.metadata->at(data.name)->definePrimitive(data.size);
			}
			variant_case(vm::code::PointerType, data) {
				tctx.metadata->at(data.name)->definePointer(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::StaticTableType, data) {
				tctx.metadata->at(data.name)->defineStaticTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner),
					data.table_size
				);
			}
			variant_case(vm::code::DynamicTableType, data) {
				tctx.metadata->at(data.name)->defineDynamicTable(
					tctx.metadata->atMaybe(data.inner).expect<UnknownSubtypeError>(data, data.inner)
				);
			}
			variant_case(vm::code::DataType, data) {
				std::vector<std::pair<base::StrID, TypeRef>> fields;
				fields.reserve(data.fields.size());
				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<UnknownSubtypeError>(data, field.name)
					);
				tctx.metadata->at(data.name)->defineData(fields, {});
			}
			variant_case(vm::code::VariantType, data) {
				std::vector<vm::TypeRef> variants;
				variants.reserve(data.variant_alternatives.size());
				for (auto& variant: data.variant_alternatives)
					variants.emplace_back(
						tctx.metadata->atMaybe(variant).expect<UnknownSubtypeError>(data, variant)
					);
				tctx.metadata->at(data.name)->defineVariant(variants);
			}
			variant_case(vm::code::FunctionType, data) {
				std::vector<vm::TypeCRef> parameters;
				parameters.reserve(data.parameters.size());
				for (auto& param: data.parameters)
					parameters.emplace_back(tctx.metadata->at(param));
				tctx.metadata->at(data.name)->defineFunction(
					parameters,
					tctx.metadata->atMaybe(data.result).expect<UnknownSubtypeError>(data, data.result)
				);
			}
			variant_case(vm::code::OpaqueType, opaque) {
				tctx.metadata->at(opaque.name)->defineOpaque(opaque.size);
			}
			variant_case(vm::code::ClassType, data) {
				// TODO: Verify that all virtuals are implemented and only them.
				// TODO: Verify that vmethods types are the same as implementations. Add
				// some tests.

				// @note: First field in the data types is always a vt.
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				fields.reserve(data.fields.size() + 1);

				for (auto& field: data.fields)
					fields.emplace_back(
						field.name,
						tctx.metadata->atMaybe(field.type)
							.expect<UnknownSubtypeError>(data, field.name)
					);


				TypeRef tp       = tctx.metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};


				auto kind = InheritanceMetadata::Class{
					.is_abstract = data.is_abstract,
					.extends     = data.extends.map(get_type),
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();

				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods) {
					if (virtual_methods.contains(method.name))
						throw DuplicatedVirtualMethodError(type);
					virtual_methods.put(method.name, get_type(method.type));
				}

				base::HashMap<base::StrID, TypeCRef> vmethods_implementations;
				for (auto& method: data.vmethods_implementations) {
					if (vmethods_implementations.contains(method.name))
						throw DuplicatedVirtualMethodImplementationError(type);
					vmethods_implementations.put(method.name, get_type(method.type));
				}

				vm::InheritanceMetadata inheritance_metadata(
					tp,
					kind,
					std::move(implements),
					std::move(virtual_methods),
					std::move(vmethods_implementations)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_case(vm::code::InterfaceType, data) {
				// TODO: Verify that all virtuals are implemented and only them.
				std::vector<std::pair<base::StrID, TypeRef>> fields{
					{ base::StrID("vt"), to_low_type(SpecialTypes::get().vtable_ptr) }
				};
				TypeRef tp       = tctx.metadata->at(data.name);
				auto    get_type = [&](base::StrID name) -> TypeCRef {
                    return tctx.getMetadata().atMaybe(name).expect<UnknownSubtypeError>(data, name);
				};
				auto implements = data.implements | std::views::transform(get_type)
				                | std::ranges::to<std::vector>();

				// TODO: Validate duplicate virtuals.
				base::HashMap<base::StrID, TypeCRef> virtual_methods;
				for (auto& method: data.virtual_methods)
					virtual_methods.put(method.name, get_type(method.type));

				// TODO: Validate duplicate implementations.
				base::HashMap<base::StrID, TypeCRef> vmethods_implementations;
				for (auto& method: data.vmethods_implementations)
					vmethods_implementations.put(method.name, get_type(method.type));

				vm::InheritanceMetadata inheritance_metadata(
					tp,
					InheritanceMetadata::Interface{},
					std::move(implements),
					std::move(virtual_methods),
					std::move(vmethods_implementations)
				);
				tp->defineData(fields, std::move(inheritance_metadata));
			}
			variant_default { CORE_PANIC("bad type"); }
		}
	}
	tctx.metadata->finalize();
	return tctx;
}

const vm::StableTypeIdNameMap<vm::code::TypeOfData>& TypeContext::getTypes() const { return types; }

const vm::TypeMetadata& TypeContext::getMetadata() const { return *metadata; }

Box<vm::TypeMetadata> TypeContext::moveMetadata() && { return std::move(metadata); }

void TypeContextBuilder::addType(const TypeOfData& type) {
	const auto name = VISIT(type, tp, return tp.name);
	match_optional(types.atMaybe(name)) {
		opt_some(tp) {
			if (type != *tp) throw DuplicatedTypeError(type);
		}
		opt_none { types.insert(type, name); }
	}
}
