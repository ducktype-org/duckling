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
	};
}
