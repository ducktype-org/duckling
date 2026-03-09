#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <unordered_set>

namespace vm::code::valid_type {
	/**
	 * @brief Temporary representation of a type during definition phase. This is used to store
	 * data about a type before we have all the information needed to finalize it, like type
	 * sizes for structure fields or inheritance metadata. After we have all the information
	 * needed to finalize a type, we convert this to a concrete type during finalization.
	 */
	namespace defined {
		/**
		 * @brief Primitive type representation.
		 */
		struct DefinedPrimitive final {
			Bytes size;
		};

		/**
		 * @brief Pointer type representation.
		 */
		struct DefinedPointer final {
			ValidTypeID inner;

			DefinedPointer(ValidTypeID inner): inner(inner) {}
		};

		/**
		 * @brief Fixed-size table type representation.
		 */
		struct DefinedFixedSizeTable final {
			ValidTypeID inner;
			usize       element_count;
		};

		/**
		 * @brief Dynamic table type representation.
		 */
		struct DefinedDynamicTable final {
			ValidTypeID inner;
		};

		/**
		 * @brief A field inside a Structure type.
		 */
		struct DefinedField final {
			base::StrID name;
			ValidTypeID type;
		};

		struct InheritanceDefinitionData final {
			/**
			 * @brief In an interface-like way. Multiple interfaces allowed.
			 */
			std::unordered_set<ValidTypeID> implements;

			/**
			 * @brief Virtual methods declared just in this structure.
			 */
			base::HashMap<base::StrID, ValidTypeID> new_virtual_methods;

			/**
			 * @brief New implementations provided in this structure.
			 */
			base::HashMap<base::StrID, base::StrID> implementations;

			/**
			 * @brief Class kind. If this type is a class, contains information about its superclass
			 * and whether it's abstract.
			 */
			struct ClassKind final {
				/**
				 * @brief A super-class. Only one super-class allowed.
				 */
				base::Optional<ValidTypeID> extends;

				/**
				 * @brief Whether this type is abstract (i.e. cannot be instantiated).
				 */
				bool is_abstract = false;
			};

			/**
			 * @brief Interface kind. If this type is an interface, then no additional data is needed.
			 */
			struct InterfaceKind final {};

			std::variant<InterfaceKind, ClassKind> kind;
		};

		/**
		 * @brief Struct/Class (data) type representation.
		 */
		struct DefinedStructure final {
			std::vector<DefinedField> field_definitions;

			base::Optional<InheritanceDefinitionData> forwarded_inheritance_data;
		};

		/**
		 * @brief Variant type representation.
		 */
		struct DefinedVariant final {
			std::vector<ValidTypeID>
				alternatives;  /// Order matters, as it determines type tag values.
		};

		/**
		 * @brief Function type representation.
		 */
		struct DefinedFunction final {
			std::vector<ValidTypeID> parameters;
			ValidTypeID              result;
		};

		/**
		 * @brief Opaque type representation. This is used for types whose actual content is not
		 * known, like external types.
		 */
		struct DefinedOpaque final {
			Bytes size;
		};
	}

#define DEFINED_TYPE_LIST                                                                 \
	defined::DefinedPrimitive, defined::DefinedPointer, defined::DefinedFixedSizeTable,   \
		defined::DefinedDynamicTable, defined::DefinedStructure, defined::DefinedVariant, \
		defined::DefinedFunction, defined::DefinedOpaque

	template<class T>
	concept DefinedType = base::IsOneOf<T, DEFINED_TYPE_LIST>;

	using DefinedTypeVariant = std::variant<std::monostate, DEFINED_TYPE_LIST>;
}
