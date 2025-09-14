#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

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

		const ObjIdNameMap<TypeOfData>& getCurrentTypes() const;

		/**
		 * @brief Creates TypeMetadata by building types.
		 */
		// TODOP: Add doc comment.
		Box<TypeMetadata> validateAndProduceTypeMetadata(
			const base::HashMap<base::StrID, FuncSignature>& available_functions
		) const;

	private:
		ObjIdNameMap<TypeOfData> types;
	};
}
