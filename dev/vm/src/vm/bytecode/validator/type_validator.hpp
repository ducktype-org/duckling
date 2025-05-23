#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief TypeContext allows for first adding a set of types,
	 * and then validating and building them.
	 */
	class TypeContext {
	public:
		/**
		 * @brief Inserts a new type. If a type is duplicated throws DuplicatedTypeError.
		 */
		void insertType(const TypeOfData& type);

		const StableTypeIdNameMap<TypeOfData>& getCurrentTypes() const;

		/**
		 * @brief Creates TypeMetadata by building types.
		 */
		Box<TypeMetadata> validateAndProduceTypeMetadata() const;

	private:
		using FieldVector = std::vector<std::pair<base::StrID, TypeRef>>;
		StableTypeIdNameMap<TypeOfData> types;

		/**
		 * @brief Throws a builder error if type is invalid in current context.
		 */
		void validateType(const TypeOfData& type) const;

		/**
		 * @brief Throws a builder error if types are invalid in current context.
		 * Checks each type individually and inheritance
		 * hierarchy soundness.
		 */
		void validateTypes() const;

		/**
		 * @brief Retrieves the TypeOfData variant for a given type name, throws UnknownSubtypeError
		 * if the type is not found.
		 */
		const vm::code::TypeOfData& getTypeOfData(base::StrID name, const TypeOfData& context) const;

		/**
		 * @brief Retrieves a specific type from a TypeOfData variant, throws UnknownSubtypeError if
		 * the type is not found and an error returned by the error factory if the expected type
		 * mismatches the actual one.
		 */
		template<typename ExpectedType, typename ErrorFactory>
		const ExpectedType& getType(
			base::StrID name, const vm::code::TypeOfData& context, ErrorFactory error_factory
		) const;

		/**
		 * @brief Recursively collects fields from a FieldableType (ClassType or DataType) and its
		 * superclasses.
		 */
		template<typename FieldableType>
		void collectFieldsRecursive(
			const FieldableType&                     fieldable,
			const TypeOfData&                        error_context,
			base::HashMap<base::StrID, base::StrID>& fields
		) const;

		/**
		 * @brief Recursively collects virtual methods from an InheritableType (ClassType or
		 * InterfaceType), its implemented interfaces, and superclasses (for ClassType).
		 */
		template<typename InheritableType, typename ErrorContextType>
		void collectVirtualMethodsRecursive(
			const InheritableType&                   inh,
			const ErrorContextType&                  error_context_inh,
			base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Recursively collects method implementations from an InheritableType (ClassType or
		 * InterfaceType), its implemented interfaces, and superclasses.
		 */
		template<typename InheritableType, typename ErrorContextType>
		void collectImplementationsRecursive(
			const InheritableType&                   inh,
			const ErrorContextType&                  error_context_inh,
			base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that a method's first argument is a pointer to the 'this' object.
		 */
		template<typename InheritableType>
		void validateMethodFirstArgument(const InheritableType& inh, const FunctionType& func_type)
			const;

		/**
		 * @brief Validates that a method implementation's signature matches a virtual method's
		 * signature.
		 */
		template<typename InheritableType>
		void validateMethodSignatureMatch(
			const InheritableType& inh,
			const FunctionType&    vmethod_type,
			const FunctionType&    impl_type
		) const;

		/**
		 * @brief Validates the signatures of all virtual methods declared in an InheritableType.
		 */
		template<typename InheritableType>
		void validateVMethodSignatures(const InheritableType& inh) const;

		/**
		 * @brief Validates all method implementations within an InheritableType.
		 */
		template<typename InheritableType>
		void validateImplementations(
			const InheritableType&                         inh,
			const base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that all virtual methods of a non-abstract class are implemented.
		 */
		void validateAllMethodsImplemented(
			const ClassType& clazz, const base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that there are no duplicate field names within a FieldableType and its
		 * hierarchy.
		 */
		template<typename FieldableType>
		void validateFieldDuplicates(const FieldableType& fieldable) const;

		/**
		 * @brief Validates that an InheritableType does not list the same interface multiple times
		 * in its direct `implements` list.
		 */
		template<typename InheritableType>
		void validateImplementsDuplicates(const InheritableType& implements) const;

		/**
		 * @brief Recursively builds a vtable layout for an InheritableType.
		 */
		template<typename InheritableType, typename ErrorContextType>
		void buildVTableRecursive(
			const InheritableType&                inh,
			const ErrorContextType&               error_context_inh,
			base::HashMap<base::StrID, TypeCRef>& implementations,
			TypeMetadata&                         metadata
		) const;

		/**
		 * @brief Builds a vector of fields (FieldVector) for an InheritableType. Collects all
		 * fields from superclasses.
		 */
		template<typename FieldableType>
		FieldVector buildFieldVector(const FieldableType& inh, TypeMetadata& metadata) const;

		/**
		 * @brief Builds inheritance metadata for a given inheritable. Builds a field vector and a
		 * vtable.
		 */
		template<typename InheritableType>
		vm::InheritanceMetadata buildInheritanceMetadata(
			const InheritableType& inh, TypeMetadata& metadata
		) const;
	};
}
