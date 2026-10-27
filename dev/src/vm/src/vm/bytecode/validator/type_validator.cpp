// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "type_validator.hpp"

#include <base/config/target_info.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_type/type_utils.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace {
	using namespace vm::code::detail;

/**
 * @brief Retrieves a type with a given name from the `TypeContext` and checks if it has an
 * expected type. If yes, it retrieves this type from the variant and returns it. If not, throws
 * an error given by the error_factory function.
 *
 * @param ctx type context to retrieve the type from.
 * @param name name of the type to retrieve.
 * @param context_for_error a type needed to throw the UnknownSubtypeError. This is the type for
 * which subtype we're looking for.
 * @param error_factory a function which returns the error to be thrown in case of type
 * mismatch. For example, if a type specified by the name in clazz.extends is not a ClassType,
 * the error_factory should return an InvalidExtendsError.
 *
 * @note This function causes a dangling reference warning, which I strongly believe is a false
 * positive, thus the pragmas.
 */
#if BASE_TARGET_COMPILER_GCC
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif
	template<TypeOfDataConcept ExpectedType, ErrorFactoryConcept ErrorFactory>
	const ExpectedType& getType(
		const vm::ObjIdNameMap<TypeOfData>& tod_types,
		const base::StrID&                  name,
		const TypeOfData&                   context_for_error,
		const ErrorFactory&                 error_factory
	) {
		const TypeOfData& type_of_data
			= *tod_types.atMaybe(name).expect<UnknownSubtypeError>(context_for_error, name);
		if (const auto* specific_type = std::get_if<ExpectedType>(&type_of_data))
			return *specific_type;
		throw error_factory();
	}
#if BASE_TARGET_COMPILER_GCC
	#pragma GCC diagnostic pop
#endif

	/**
	 * @brief Recursively collects fields from a FieldableType (ClassType or DataType) and its
	 * superclasses and inserts them into the `fields` map.
	 */
	template<FieldableTypeConcept FieldableType>
	void insertFieldsRecursive(
		base::HashMap<base::StrID, base::StrID>& fields,
		const FieldableType&                     fieldable,
		const TypeOfData&                        error_context,
		const vm::ObjIdNameMap<TypeOfData>&      tod_types
	) {
		if constexpr (std::is_same_v<FieldableType, ClassType>) {
			if (fieldable.extends) {
				const auto& super_class = getType<ClassType>(
					tod_types,
					fieldable.extends->canonicalName(),
					error_context,
					[&]() {
						return InvalidExtendsError(fieldable, fieldable.extends->canonicalName());
					}
				);
				insertFieldsRecursive(fields, super_class, error_context, tod_types);
			}
		}
		for (const auto& field_type: fieldable.fields) {
			if (fields.contains(field_type.name))
				throw DuplicatedFieldError(error_context, field_type.name);
			fields.put(field_type.name, field_type.type.canonicalName());
		}
	}

	/**
	 * @brief Recursively collects virtual methods from an InheritableType (ClassType or
	 * InterfaceType), its implemented interfaces, and superclasses (for ClassType) and inserts them
	 * into the `virtual_methods` map.
	 */
	template<InheritableTypeConcept InheritableType, TypeOfDataConcept ErrorContextType>
	void insertVirtualMethodsRecursive(
		base::HashMap<base::StrID, base::StrID>& virtual_methods,
		const InheritableType&                   inh,
		const ErrorContextType&                  error_context_inh,
		const vm::ObjIdNameMap<TypeOfData>&      tod_types
	) {
		for (const auto& interface_name: inh.implements) {
			const auto& interface = getType<InterfaceType>(
				tod_types,
				interface_name.canonicalName(),
				inh,
				[&]() { return InvalidImplementsError(inh, interface_name.canonicalName()); }
			);
			insertVirtualMethodsRecursive(virtual_methods, interface, error_context_inh, tod_types);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class = getType<ClassType>(
					tod_types,
					inh.extends->canonicalName(),
					error_context_inh,
					[&]() { return InvalidExtendsError(inh, inh.extends->canonicalName()); }
				);
				insertVirtualMethodsRecursive(
					virtual_methods, super_class, error_context_inh, tod_types
				);
			}
		}
		for (const auto& vmethod: inh.virtual_methods) {
			if (virtual_methods.contains(vmethod.name))
				throw DuplicatedVirtualMethodError(error_context_inh, vmethod.name);
			virtual_methods.put(vmethod.name, vmethod.type.canonicalName());
		}
	}

	/**
	 * @brief Recursively collects method implementations from an InheritableType (ClassType or
	 * InterfaceType), its implemented interfaces, and superclasses and inserts them into the
	 * `implementations` map.
	 */
	template<InheritableTypeConcept InheritableType, TypeOfDataConcept ErrorContextType>
	void insertImplementationsRecursive(
		base::HashMap<base::StrID, base::StrID>& implementations,
		const InheritableType&                   inh,
		const ErrorContextType&                  error_context_inh,
		const vm::ObjIdNameMap<TypeOfData>&      tod_types
	) {
		for (const auto& impl: inh.implementations)
			implementations.put(impl.name, impl.function_name);
		for (const auto& interface_name: inh.implements) {
			const auto& interface = getType<InterfaceType>(
				tod_types,
				interface_name.canonicalName(),
				error_context_inh,
				[&]() { return InvalidImplementsError(inh, interface_name.canonicalName()); }
			);
			insertImplementationsRecursive(implementations, interface, error_context_inh, tod_types);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class = getType<ClassType>(
					tod_types,
					inh.extends->canonicalName(),
					error_context_inh,
					[&]() { return InvalidExtendsError(inh, inh.extends->canonicalName()); }
				);
				insertImplementationsRecursive(
					implementations, super_class, error_context_inh, tod_types
				);
			}
		}
	}

	/**
	 * @brief The declared types whose size the size of `expr` depends on, which a cycle through
	 * a reference to it would continue into.
	 * @note None for a pointer or a dynamic table: neither one's size depends on what it is
	 * built from, which is what makes self-referential types legal.
	 */
	std::vector<base::StrID> recursesInto(const TypeExpr& expr) {
		variant_match(expr.getKind()) {
			variant_case(expr::Named, named) { return { named.name }; }
			variant_case(expr::FixedSizeTable, fixed_size_table) {
				return recursesInto(fixed_size_table.element);
			}
			variant_case(expr::Variant, variant) {
				std::vector<base::StrID> names;
				for (const auto& alternative: variant.alternatives)
					std::ranges::copy(recursesInto(alternative), std::back_inserter(names));
				return names;
			}
			variant_default { return {}; }
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Whether every name an expression is built from is declared.
	 * @note A derived type needs no declaration of its own, so only what it is built from is
	 * checked.
	 */
	bool isDeclared(const vm::ObjIdNameMap<TypeOfData>& tod_types, const TypeExpr& expr) {
		variant_match(expr.getKind()) {
			variant_case(expr::Named, named) { return tod_types.contains(named.name); }
			variant_case(expr::Pointer, pointer) { return isDeclared(tod_types, pointer.pointee); }
			variant_case(expr::CPointer, cpointer) {
				return cpointer.pointee.empty() || isDeclared(tod_types, *cpointer.pointee);
			}
			variant_case(expr::DynamicTable, dynamic_table) {
				return isDeclared(tod_types, dynamic_table.element);
			}
			variant_case(expr::FixedSizeTable, fixed_size_table) {
				return isDeclared(tod_types, fixed_size_table.element);
			}
			variant_case(expr::Variant, variant) {
				return std::ranges::all_of(variant.alternatives, [&](const TypeExpr& alternative) {
					return isDeclared(tod_types, alternative);
				});
			}
		}
		CORE_UNREACHABLE();
	}

	/// @brief Whether `expr` is a managed pointer to the type named `pointee`.
	bool isPointerTo(const TypeExpr& expr, base::StrID pointee) {
		return expr == TypeExpr::pointer(TypeExpr::type(pointee));
	}

	/**
	 * @brief Validates that a method's FunctionType first parameter is a pointer to the 'this'
	 * object.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateMethodFirstArgument(const InheritableType& inh, const FunctionType& func_type) {
		if (func_type.parameters.empty()) throw MethodFirstArgumentError(inh, func_type.name);
		if (!isPointerTo(func_type.parameters[0], inh.name))
			throw MethodFirstArgumentError(inh, func_type.name);
	}

	/**
	 * @brief Validates that a function signature's first argument is a pointer to the 'this' object.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateMethodFirstArgumentImpl(
		const InheritableType& inh, const FuncSignature& func_signature, base::StrID func_name
	) {
		if (func_signature.parameters.empty()) throw MethodFirstArgumentError(inh, func_name);
		if (!isPointerTo(func_signature.parameters[0].expr, inh.name))
			throw MethodFirstArgumentError(inh, func_name);
	}

	/**
	 * @brief Validates that a method implementation's signature matches a virtual method's
	 * signature.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateMethodSignatureMatchImpl(
		const InheritableType& inh,
		const FunctionType&    vmethod_type,
		const FuncSignature&   impl_signature,
		base::StrID            impl_type_name
	) {
		if (vmethod_type.result.size() != impl_signature.result_types.size())
			throw MethodTypeError(inh, impl_type_name);
		for (u64 i = 0; i < impl_signature.result_types.size(); i++)
			if (vmethod_type.result[i].canonicalName()
			    != impl_signature.result_types[i].expr.canonicalName())
				throw MethodTypeError(inh, impl_type_name);

		if (vmethod_type.parameters.size() != impl_signature.parameters.size())
			throw MethodTypeError(inh, impl_type_name);
		for (u64 i = 1; i < impl_signature.parameters.size(); i++)
			if (vmethod_type.parameters[i].canonicalName()
			    != impl_signature.parameters[i].expr.canonicalName())
				throw MethodTypeError(inh, impl_type_name);
	}

	/**
	 * @brief Validates the signatures of all virtual methods declared in an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateVMethodSignatures(
		const InheritableType& inh, const vm::ObjIdNameMap<TypeOfData>& tod_types
	) {
		for (const auto& vmethod: inh.virtual_methods) {
			const auto& vmethod_type
				= getType<FunctionType>(tod_types, vmethod.type.canonicalName(), inh, [&]() {
					  return TypeIsNotFunctionalError(vmethod.type.canonicalName());
				  });
			validateMethodFirstArgument(inh, vmethod_type);
		}
	}

	/**
	 * @brief Validates all method implementations within an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateImplementations(
		const InheritableType&                           inh,
		const base::HashMap<base::StrID, base::StrID>&   virtual_methods,
		const vm::ObjIdNameMap<TypeOfData>&              tod_types,
		const base::HashMap<base::StrID, FuncSignature>& functions
	) {
		base::HashMap<base::StrID, base::StrID> implementations;
		for (const auto& implementation: inh.implementations) {
			if (implementations.contains(implementation.name))
				throw DuplicatedVirtualMethodImplementationError(inh, implementation.name);
			implementations.put(implementation.name);

			if (!virtual_methods.contains(implementation.name))
				throw InvalidVirtualMethodImplementationError(inh, implementation.name);

			const auto& vmethod_name = virtual_methods[implementation.name];
			const auto& impl_name    = implementation.function_name;
			const auto& vmethod_type = getType<FunctionType>(tod_types, vmethod_name, inh, [&]() {
				return TypeIsNotFunctionalError(vmethod_name);
			});
			if (!functions.contains(impl_name))
				throw InvalidVirtualMethodImplementationError(inh, implementation.name);

			const auto& impl_signature = functions.at(impl_name);
			validateMethodFirstArgumentImpl(inh, impl_signature, impl_name);
			validateMethodSignatureMatchImpl(inh, vmethod_type, impl_signature, impl_name);
		}
	}

	/**
	 * @brief Validates that all virtual methods of a non-abstract class are implemented on the
	 * whole inheritance path.
	 */
	void validateAllMethodsImplemented(
		const ClassType&                               clazz,
		const base::HashMap<base::StrID, base::StrID>& virtual_methods,
		const vm::ObjIdNameMap<TypeOfData>&            tod_types
	) {
		base::HashMap<base::StrID, base::StrID> implementations;
		insertImplementationsRecursive(implementations, clazz, clazz, tod_types);

		for (auto& [name, _]: virtual_methods)
			if (!implementations.contains(name)) throw UnimplementedVirtualMethodError(clazz, name);
	}

	/**
	 * @brief Validates that there are no duplicate field names within a FieldableType and its
	 * hierarchy and that field types exist in the context.
	 */
	template<FieldableTypeConcept FieldableType>
	void validateFieldDuplicatesAndSubtypeExistence(
		const FieldableType& fieldable, const vm::ObjIdNameMap<TypeOfData>& tod_types
	) {
		for (auto& field: fieldable.fields)
			if (!isDeclared(tod_types, field.type))
				throw UnknownSubtypeError(fieldable, field.type.canonicalName());

		base::HashMap<base::StrID, base::StrID> fields;
		insertFieldsRecursive(fields, fieldable, fieldable, tod_types);
	}

	/**
	 * @brief Validates that an InheritableType does not list the same interface multiple times
	 * in its direct `implements` list.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateImplementsDuplicates(const InheritableType& inh) {
		base::HashMap<base::StrID, base::StrID> interfaces;
		for (const auto& impl: inh.implements) {
			const auto impl_name = impl.canonicalName();
			if (interfaces.contains(impl_name)) throw DuplicatedImplementsError(inh, impl_name);
			interfaces.put(impl_name);
		}
	}

	/**
	 * @brief Throws a builder error if type is invalid in current context.
	 */
	void validateType(
		const TypeOfData&                                type,
		const vm::ObjIdNameMap<TypeOfData>&              tod_types,
		const base::HashMap<base::StrID, FuncSignature>& functions
	) {
		variant_match(type) {
			variant_case(PrimitiveType, primitive) {
				if (primitive.size == Bytes{ 0 }) throw InvalidPrimitiveSizeError(primitive);
			}
			variant_case(FunctionType, function) {
				for (auto& param: function.parameters)
					if (!isDeclared(tod_types, param))
						throw UnknownSubtypeError(function, param.canonicalName());

				for (auto& reslt: function.result)
					if (!isDeclared(tod_types, reslt))
						throw UnknownSubtypeError(function, reslt.canonicalName());
			}
			variant_case(DataType, data) {
				validateFieldDuplicatesAndSubtypeExistence(data, tod_types);
			}
			variant_case(InterfaceType, interface) {
				base::HashMap<base::StrID, base::StrID> virtual_methods;
				insertVirtualMethodsRecursive(virtual_methods, interface, interface, tod_types);

				validateImplementsDuplicates(interface);
				validateVMethodSignatures(interface, tod_types);
				validateImplementations(interface, virtual_methods, tod_types, functions);
			}
			variant_case(ClassType, clazz) {
				// All virtual methods that can be implemented by this class (including superclass
				// and interface vmethods as well).
				base::HashMap<base::StrID, base::StrID> virtual_methods;
				insertVirtualMethodsRecursive(virtual_methods, clazz, clazz, tod_types);

				validateFieldDuplicatesAndSubtypeExistence(clazz, tod_types);
				validateImplementsDuplicates(clazz);
				validateVMethodSignatures(clazz, tod_types);
				validateImplementations(clazz, virtual_methods, tod_types, functions);
				if (!clazz.is_abstract)
					validateAllMethodsImplemented(clazz, virtual_methods, tod_types);
			}
			variant_case_novalue(OpaqueType) {}
			variant_default {
				CORE_PANIC("Unhandled type during type validation: ", typeToString(type));
			}
		}
	}

	void validateHierarchyAcyclic(const vm::ObjIdNameMap<TypeOfData>& tod_types) {
		// Check for cycles in hierarchy.
		enum Status { Waiting, Visited, Done };

		base::HashMap<base::StrID, Status> status;
		for (const auto& [_t, _id, name]: tod_types.allData()) status.put(name, Waiting);

		auto helper = [&](this auto self, const auto& type) {
			auto name = typeName(type);
			if (status[name] == Visited)
				throw CycleInHierarchyError(type);
			else if (status[name] == Done)
				return;

			status[name] = Visited;
			variant_match(type) {
				variant_case(ClassType, clazz) {
					if_opt_some(clazz.extends, superclass) {
						if (!isDeclared(tod_types, superclass))
							throw InvalidExtendsError(clazz, superclass.canonicalName());
						for (const auto next: recursesInto(superclass)) self(*tod_types.at(next));
					}
					for (auto iface: clazz.implements) {
						if (!isDeclared(tod_types, iface))
							throw InvalidImplementsError(clazz, iface.canonicalName());
						for (const auto next: recursesInto(iface)) self(*tod_types.at(next));
					}
				}
				variant_case(InterfaceType, interface) {
					for (auto iface: interface.implements) {
						if (!isDeclared(tod_types, iface))
							throw InvalidImplementsError(interface, iface.canonicalName());
						for (const auto next: recursesInto(iface)) self(*tod_types.at(next));
					}
				}
			}
			status[name] = Done;
		};
		for (const auto& [_t, _id, name]: tod_types.allData()) helper(*tod_types.at(name));
	}

	void validateDefinitionsAcyclic(const vm::ObjIdNameMap<TypeOfData>& tod_types) {
		// Check for cycles in definitions.
		enum Status { Waiting, Visited, Done };

		base::HashMap<base::StrID, Status> status;
		for (const auto& [_t, _id, name]: tod_types.allData()) status.put(name, Waiting);

		auto helper = [&](this auto self, const TypeOfData& type) {
			auto name = typeName(type);
			if (status[name] == Visited)
				throw CyclicDependencyError(type);
			else if (status[name] == Done)
				return;

			status[name] = Visited;
			variant_match(type) {
				variant_case(PrimitiveType, primitive) {}
				variant_case(InterfaceType, interface) {}
				variant_case(ClassType, clazz) {
					for (auto& field: clazz.fields) {
						if (!isDeclared(tod_types, field.type))
							throw UnknownSubtypeError(clazz, field.type.canonicalName());
						for (const auto next: recursesInto(field.type)) self(*tod_types.at(next));
					}
				}
				variant_case(DataType, data) {
					for (auto& field: data.fields) {
						if (!isDeclared(tod_types, field.type))
							throw UnknownSubtypeError(data, field.type.canonicalName());
						for (const auto next: recursesInto(field.type)) self(*tod_types.at(next));
					}
				}
				variant_case(FunctionType, function) {
					for (auto& param: function.parameters)
						if (!isDeclared(tod_types, param))
							throw UnknownSubtypeError(function, param.canonicalName());
					for (auto& reslt: function.result)
						if (!isDeclared(tod_types, reslt))
							throw UnknownSubtypeError(function, reslt.canonicalName());
					for (auto& param: function.parameters)
						for (const auto next: recursesInto(param)) self(*tod_types.at(next));
					for (auto& reslt: function.result)
						for (const auto next: recursesInto(reslt)) self(*tod_types.at(next));
				}
				variant_case(OpaqueType, opaque) {}
				variant_default {
					CORE_PANIC("Unhandled type during definition cycle check: ", typeToString(type));
				}
			}
			status[name] = Done;
		};
		for (const auto& [_t, _id, name]: tod_types.allData()) helper(*tod_types.at(name));
	}

	/**
	 * @brief Checks if a method name is deterministic, i.e. that there are no two methods with the
	 * same name in the whole program. This is needed to ensure that method lookup by name works
	 * correctly.
	 */
	void validateMethodNameIsDeterministic(const vm::ObjIdNameMap<TypeOfData>& tod_types) {
		// Maps method name to type name of the signature.
		// All we need is to check the type names,
		// because implementations must have as the first parameter a pointer to the object type,
		// so this guarantees that they also come from the same tree.
		base::HashMap<base::StrID, base::StrID> method_signatures;
		for (const auto& [_t, _id, name]: tod_types.allData()) {
			const auto& type = *tod_types.at(name);
			variant_match(type) {
				variant_case(ClassType, clazz) {
					for (const auto& vmethod: clazz.virtual_methods) {
						if (auto method_signature = method_signatures.atMaybe(vmethod.name);
						    method_signature && **method_signature != vmethod.type.canonicalName())
							throw DuplicatedMethodNameError(type, vmethod.name);
						method_signatures.put(vmethod.name, vmethod.type.canonicalName());
					}
				}
				variant_case(InterfaceType, interface) {
					for (const auto& vmethod: interface.virtual_methods) {
						if (auto method_signature = method_signatures.atMaybe(vmethod.name);
						    method_signature && **method_signature != vmethod.type.canonicalName())
							throw DuplicatedMethodNameError(type, vmethod.name);
						method_signatures.put(vmethod.name, vmethod.type.canonicalName());
					}
				}
			}
		}
	}

	void validateTypeIntegrity(const vm::ObjIdNameMap<TypeOfData>& tod_types) {
		validateDefinitionsAcyclic(tod_types);
		validateHierarchyAcyclic(tod_types);
	}

}

void vm::code::detail::validateTypes(
	const ObjIdNameMap<TypeOfData>&                  tod_types,
	const std::vector<usize>&                        new_types_id,
	const base::HashMap<base::StrID, FuncSignature>& functions
) {
	validateTypeIntegrity(tod_types);
	for (usize type_id: new_types_id) validateType(*tod_types.at(type_id), tod_types, functions);

	validateMethodNameIsDeterministic(tod_types);
}
