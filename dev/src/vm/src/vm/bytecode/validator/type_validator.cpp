#include "type_validator.hpp"

#include "vm/bytecode/validator/type.hpp"
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_utils.hpp>

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
#if defined(__GNUG__) && !defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif
	template<type::ConcreteType ExpectedType, ErrorFactoryConcept ErrorFactory>
	const ExpectedType& getType(
		const TypeContext&  ctx,
		const base::StrID&  name,
		const TypeOfData&   context_for_error,
		const ErrorFactory& error_factory
	) {
		const type::Type& tp = *ctx.getCurrentTypes().atMaybe(name).expect<UnknownSubtypeError>(
			context_for_error, name
		);
		if (const auto& specific_type = tp.maybeGet<ExpectedType>()) return *specific_type;
		throw error_factory();
	}
#if defined(__GNUG__) && !defined(__clang__)
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
		const TypeContext&                       ctx
	) {
		if constexpr (std::is_same_v<FieldableType, ClassType>) {
			if (fieldable.extends) {
				const auto& super_class
					= getType<ClassType>(ctx, *fieldable.extends, error_context, [&]() {
						  return InvalidExtendsError(fieldable, *fieldable.extends);
					  });
				insertFieldsRecursive(fields, super_class, error_context, ctx);
			}
		}
		for (const auto& field_type: fieldable.fields) {
			if (fields.contains(field_type.name))
				throw DuplicatedFieldError(error_context, field_type.name);
			fields.put(field_type.name, field_type.type);
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
		const TypeContext&                       ctx
	) {
		for (const auto& interface_name: inh.implements) {
			const auto& interface = getType<InterfaceType>(ctx, interface_name, inh, [&]() {
				return InvalidImplementsError(inh, interface_name);
			});
			insertVirtualMethodsRecursive(virtual_methods, interface, error_context_inh, ctx);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class
					= getType<ClassType>(ctx, *inh.extends, error_context_inh, [&]() {
						  return InvalidExtendsError(inh, *inh.extends);
					  });
				insertVirtualMethodsRecursive(virtual_methods, super_class, error_context_inh, ctx);
			}
		}
		for (const auto& vmethod: inh.virtual_methods) {
			if (virtual_methods.contains(vmethod.name))
				throw DuplicatedVirtualMethodError(error_context_inh, vmethod.name);
			virtual_methods.put(vmethod.name, vmethod.type);
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
		const TypeContext&                       ctx
	) {
		for (const auto& impl: inh.implementations) implementations.put(impl.name, impl.type);
		for (const auto& interface_name: inh.implements) {
			const auto& interface = getType<InterfaceType>(
				ctx, interface_name, error_context_inh, [&]() {
					return InvalidImplementsError(inh, interface_name);
				}
			);
			insertImplementationsRecursive(implementations, interface, error_context_inh, ctx);
		}
		if constexpr (std::is_same_v<InheritableType, ClassType>) {
			if (inh.extends) {
				const auto& super_class
					= getType<ClassType>(ctx, *inh.extends, error_context_inh, [&]() {
						  return InvalidExtendsError(inh, *inh.extends);
					  });
				insertImplementationsRecursive(implementations, super_class, error_context_inh, ctx);
			}
		}
	}

	/**
	 * @brief Validates that a method's FunctionType first parameter is a pointer to the 'this'
	 * object.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateMethodFirstArgument(
		const InheritableType& inh, const FunctionType& func_type, const TypeContext& ctx
	) {
		if (func_type.parameters.empty()) throw MethodFirstArgumentError(inh, func_type.name);
		const auto& first_param_type_name = func_type.parameters[0];
		const auto& first_param_type = getType<PointerType>(ctx, first_param_type_name, inh, [&]() {
			return MethodFirstArgumentError(inh, func_type.name);
		});
		if (first_param_type.inner != inh.name) throw MethodFirstArgumentError(inh, func_type.name);
	}

	/**
	 * @brief Validates that a function signature's first argument is a pointer to the 'this' object.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateMethodFirstArgumentImpl(
		const InheritableType& inh,
		const FuncSignature&   func_signature,
		base::StrID            func_name,
		const TypeContext&     ctx
	) {
		if (func_signature.parameters.empty()) throw MethodFirstArgumentError(inh, func_name);
		const auto& first_param_type_name = func_signature.parameters[0];
		const auto& first_param_type = getType<PointerType>(ctx, first_param_type_name, inh, [&]() {
			return MethodFirstArgumentError(inh, func_name);
		});
		if (first_param_type.inner != inh.name) throw MethodFirstArgumentError(inh, func_name);
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
		if (vmethod_type.result != impl_signature.result_type.str)
			throw MethodTypeError(inh, impl_type_name);
		if (vmethod_type.parameters.size() != impl_signature.parameters.size())
			throw MethodTypeError(inh, impl_type_name);
		for (u64 i = 1; i < impl_signature.parameters.size(); i++)
			if (vmethod_type.parameters[i] != impl_signature.parameters[i].str)
				throw MethodTypeError(inh, impl_type_name);
	}

	/**
	 * @brief Validates the signatures of all virtual methods declared in an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateVMethodSignatures(const InheritableType& inh, const TypeContext& ctx) {
		for (const auto& vmethod: inh.virtual_methods) {
			const auto& vmethod_type = getType<FunctionType>(ctx, vmethod.type, inh, [&]() {
				return TypeIsNotFunctionalError(vmethod.type);
			});
			validateMethodFirstArgument(inh, vmethod_type, ctx);
		}
	}

	/**
	 * @brief Validates all method implementations within an InheritableType.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateImplementations(
		const InheritableType&                           inh,
		const base::HashMap<base::StrID, base::StrID>&   virtual_methods,
		const TypeContext&                               ctx,
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
			const auto& impl_name    = implementation.type;
			const auto& vmethod_type = getType<FunctionType>(ctx, vmethod_name, inh, [&]() {
				return TypeIsNotFunctionalError(vmethod_name);
			});
			if (!functions.contains(impl_name))
				throw InvalidVirtualMethodImplementationError(inh, implementation.name);

			const auto& impl_signature = functions.at(impl_name);
			validateMethodFirstArgumentImpl(inh, impl_signature, impl_name, ctx);
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
		const TypeContext&                             ctx
	) {
		base::HashMap<base::StrID, base::StrID> implementations;
		insertImplementationsRecursive(implementations, clazz, clazz, ctx);

		for (auto& [name, _]: virtual_methods)
			if (!implementations.contains(name)) throw UnimplementedVirtualMethodError(clazz, name);
	}

	/**
	 * @brief Validates that there are no duplicate field names within a FieldableType and its
	 * hierarchy and that field types exist in the context.
	 */
	template<FieldableTypeConcept FieldableType>
	void validateFieldDuplicatesAndSubtypeExistence(
		const FieldableType& fieldable, const TypeContext& ctx
	) {
		for (auto& field: fieldable.fields)
			if (!ctx.getCurrentTypes().contains(field.type))
				throw UnknownSubtypeError(fieldable, field.type);

		base::HashMap<base::StrID, base::StrID> fields;
		insertFieldsRecursive(fields, fieldable, fieldable, ctx);
	}

	/**
	 * @brief Validates that an InheritableType does not list the same interface multiple times
	 * in its direct `implements` list.
	 */
	template<InheritableTypeConcept InheritableType>
	void validateImplementsDuplicates(const InheritableType& inh) {
		base::HashMap<base::StrID, base::StrID> interfaces;
		for (const auto& impl: inh.implements) {
			if (interfaces.contains(impl)) throw DuplicatedImplementsError(inh, impl);
			interfaces.put(impl);
		}
	}
}

/**
 * @brief Throws a builder error if type is invalid in current context.
 */
void vm::code::detail::validateType(
	const TypeOfData&                                type,
	const TypeContext&                               ctx,
	const base::HashMap<base::StrID, FuncSignature>& functions
) {
	auto& types = ctx.getCurrentTypes();
	variant_match(type) {
		variant_case(PrimitiveType, primitive) {
			if (primitive.size == 0) throw InvalidPrimitiveSizeError(primitive);
		}
		variant_case(PointerType, pointer) {
			if (!types.contains(pointer.inner)) throw UnknownSubtypeError(pointer, pointer.inner);
		}
		variant_case(FixedSizeTableType, fixed_table) {
			if (!types.contains(fixed_table.inner))
				throw UnknownSubtypeError(fixed_table, fixed_table.inner);
		}
		variant_case(DynamicTableType, dynamic_table) {
			if (!types.contains(dynamic_table.inner))
				throw UnknownSubtypeError(dynamic_table, dynamic_table.inner);
		}
		variant_case(FunctionType, function) {
			for (auto& param: function.parameters)
				if (!types.contains(param)) throw UnknownSubtypeError(function, param);
			if (!types.contains(function.result))
				throw UnknownSubtypeError(function, function.result);
		}
		variant_case(VariantType, variant) {
			for (auto& alternative: variant.variant_alternatives)
				if (!types.contains(alternative)) throw UnknownSubtypeError(variant, alternative);
			if (variant.variant_alternatives.empty()) throw EmptyVariantError(variant);
		}
		variant_case(DataType, data) { validateFieldDuplicatesAndSubtypeExistence(data, ctx); }
		variant_case(InterfaceType, interface) {
			base::HashMap<base::StrID, base::StrID> virtual_methods;
			insertVirtualMethodsRecursive(virtual_methods, interface, interface, ctx);

			validateImplementsDuplicates(interface);
			validateVMethodSignatures(interface, ctx);
			validateImplementations(interface, virtual_methods, ctx, functions);
		}
		variant_case(ClassType, clazz) {
			// All virtual methods that can be implemented by this class (including superclass
			// and interface vmethods as well).
			base::HashMap<base::StrID, base::StrID> virtual_methods;
			insertVirtualMethodsRecursive(virtual_methods, clazz, clazz, ctx);

			validateFieldDuplicatesAndSubtypeExistence(clazz, ctx);
			validateImplementsDuplicates(clazz);
			validateVMethodSignatures(clazz, ctx);
			validateImplementations(clazz, virtual_methods, ctx, functions);
			if (!clazz.is_abstract) validateAllMethodsImplemented(clazz, virtual_methods, ctx);
		}
		variant_case_novalue(OpaqueType) {}
		variant_default {
			CORE_PANIC("Unhandled type during type validation: ", typeToString(type));
		}
	}
}

void vm::code::detail::validateTypesIntegrity(const TypeContext& ctx) {
	// Check for cycles in hierarchy.
	enum Status { Waiting, Visited, Done };

	auto& types = ctx.getCurrentTypes();

	base::HashMap<base::StrID, Status> status;
	for (const auto& type: types) status.put(type.getName(), Waiting);

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
					if (!types.contains(superclass)) throw InvalidExtendsError(clazz, superclass);
					self(*types.at(superclass));
				}
				for (auto iface: clazz.implements) {
					if (!types.contains(iface)) throw InvalidImplementsError(clazz, iface);
					self(*types.at(iface));
				}
			}
			variant_case(InterfaceType, interface) {
				for (auto iface: interface.implements) {
					if (!types.contains(iface)) throw InvalidImplementsError(interface, iface);
					self(*types.at(iface));
				}
			}
		}
		status[name] = Done;
	};
	for (const auto& type: types) helper(type);
}
