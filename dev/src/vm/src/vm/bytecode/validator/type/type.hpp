#pragma once

#include <base/collections/optional.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/type/concrete_types.hpp>
#include <vm/bytecode/validator/type/type_id.hpp>
#include <vm/bytecode/validator/type/type_map.hpp>
#include <vm/bytecode/validator/type/type_size.hpp>

#include <variant>

namespace vm::code::valid_type {
	/**
	 * @brief Representation of a type used for validation and lowering.
	 * @note We still don't know a size of a type, because pointer size is different between safe
	 * and fast modes. This also means that we cannot calculate field offsets for structured types
	 * yet, which is why they are not stored here.
	 * @note Type is first declared with just a name and an ID, then defined with its actual
	 * content, and then finalized. Finalization is needed to detect cyclic dependencies between
	 * types. During finalization, we fill out some data like inheritance metadata for structures,
	 * because it's more effective.
	 * After finalization type is immutable. Type cannot be unfinalized.
	 */
	class Type final {
		enum class State { Declared, Defined, Finalizing, Finalized } state = State::Declared;
		// struct Declared {

		// };
		// struct Defined {

		// };
		// std::variant<Declared, Defined> state = Declared{};

	public:
		/****************/
		/* Constructors */
		/****************/
		static Type declareType(base::StrID name, TypeID id);

		void definePrimitive(Bytes size);

		void definePointer(TypeID inner);

		void defineFixedSizeTable(TypeID inner, usize element_count);

		void defineDynamicTable(TypeID inner);

		void defineData(const std::vector<std::pair<base::StrID, TypeID>>& fields_definitions);

		void defineClass(
			const std::vector<std::pair<base::StrID, TypeID>>&      fields_definitions,
			bool                                                    is_abstract,
			const base::Optional<TypeID>&                           extends,
			const std::vector<TypeID>&                              implements,
			const std::vector<std::pair<base::StrID, TypeID>>&      new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>& implementations
		);

		void defineInterface(
			const std::vector<TypeID>&                              implements,
			const std::vector<std::pair<base::StrID, TypeID>>&      new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>& implementations
		);

		void defineVariant(const std::vector<TypeID>& variant_types);

		void defineFunction(const std::vector<TypeID>& parameters, TypeID result);

		void defineOpaque(Bytes size);

		/**
		 * @brief Finalize this type. During finalization, we check for cyclic dependencies between
		 * types, and calculate some data that is needed for validation and lowering, like
		 * inheritance metadata for structures. After finalization type is immutable.
		 * @note There is no need for a type to be "unfinalizable", because we finalize all types at
		 * the end of validation, and after that we don't need to change them anymore.
		 */
		void finalize(TypeMap& types);


		/**********************/
		/* General operations */
		/**********************/

		[[nodiscard]] base::StrID getName() const;

		[[nodiscard]] TypeID getID() const;

		template<ConcreteType T>
		[[nodiscard]]
		const T& getKindAs() const {
			return std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		base::Optional<CRef<T>> maybeGetKindAs() const {
			if (!isKind<T>()) return {};
			return &std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		bool isKind() const {
			return std::holds_alternative<T>(kind);
		}

		[[nodiscard]] ConcreteTypeVariant getKind() const;

		[[nodiscard]] bool isInstantiable() const;

		[[nodiscard]] TypeSize getSize() const;

		bool operator==(const Type& other) const;

		bool operator==(const TypeID& other_id) const;

		[[nodiscard]] bool isTriviallyCopyable() const;

	private:
		/**
		 * @brief Helper function for finalize. Sets is_instantiable.
		 */
		void finalizeInstantiability(TypeMap& types);

		/**
		 * @brief Helper function for finalize. Fills inheritance metadata for structures.
		 */
		void finalizeStructureInheritanceMetadata(TypeMap& types, concrete::Structure& structure);

		/**
		 * @brief Whether this type is instantiable. This is false for types that cannot be
		 * instantiated, like void and dynamic tables or abstract classes. This flag is true for
		 * types that can be instantiated, like primitives, structures without un-instantiable
		 * fields, etc.
		 */
		bool is_instantiable = true;

		/**
		 * @brief Whether this type is trivially copyable/POD(plain old data). This is true for
		 * types that can be copied with a simple memory copy, like primitives, opaques and
		 * fixed-size tables of trivially copyable types.
		 */
		bool is_trivially_copyable = true;

		TypeSize size = TypeSize(Bytes(0), 0);

		Type(base::StrID name, TypeID id);

		base::StrID name;
		TypeID      id;

		ConcreteTypeVariant kind;
	};

}
