#pragma once

#include "type.hpp"

namespace vm {
	namespace parser {
		struct ParsedProgram;
	}

	/**
	 * @brief Holds metadata about all types in the VCPU.
	 *
	 * This class is used to store and access all types
	 * that are used in the VCPU.
	 *
	 * @TODO: Refactor TypeMetaData such that types are referenced by reference and TypeID does not
	 * exist (apart from perhaps being stored in types for some identification).
	 */
	class TypeMetadata {
	private:
		enum class TypeMetadataState { AddingTypes, Finalized };

		base::StableVector<Type>       types{};
		std::vector<TypeID>            types_ids{};
		base::Map<base::StrID, TypeID> names_to_type{};

		TypeMetadataState state = TypeMetadataState::AddingTypes;

		TypeMetadata() = default;

	public:
		TypeRef addType(Type&& type);

		/**
		 * @brief Finalize adding types.
		 */
		void finalize();

		[[nodiscard]]
		TypeCRef getType(TypeID id) const;

		// @TODO: This function is currently used by parser, but
		// should be deleted in the future
		[[nodiscard]]
		base::Optional<TypeCRef> getTypeByName(base::StrID name) const;

		friend parser::ParsedProgram;
	};
}
