#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/type/type_id.hpp>
#include <vm/bytecode/validator/type/type_size.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <unordered_set>

namespace vm::code::valid_type {
	namespace concrete {
		/**
		 * @brief Primitive type representation.
		 */
		struct Primitive final {
			Bytes size;
		};

		/**
		 * @brief Pointer type representation.
		 */
		struct Pointer final {
			ValidTypeID inner;

			Pointer(ValidTypeID inner): inner(inner) {}
		};

		/**
		 * @brief Fixed-size table type representation.
		 */
		struct FixedSizeTable final {
			ValidTypeID inner;
			usize       element_count;
		};

		/**
		 * @brief Dynamic table type representation.
		 */
		struct DynamicTable final {
			ValidTypeID inner;
		};

		/**
		 * @brief A field inside a Structure type.
		 */
		struct Field final {
			STRONG_TYPEDEF_ID_DIRECT_CREATION(ID);
			TypeSize    offset;
			base::StrID name;
			ValidTypeID type;
		};

		/**
		 * @brief Inheritance metadata for a Structure type. Contains information related to
		 * inheritance, like super types, implemented interfaces, virtual methods and vtable.
		 */
		struct InheritanceMetadata final {
			/**
			 * @brief All the types this type directly or indirectly inherits from.
			 * This is cached for easier and faster lookup during validation and lowering.
			 * @note This includes the type itself, because a type is considered to inherit from
			 * itself.
			 * @note Both a class and an interface can be a super type.
			 */
			std::unordered_set<ValidTypeID> super_types;

			/**
			 * @brief In an interface-like way. Multiple interfaces allowed.
			 */
			std::unordered_set<ValidTypeID> implements;

			/**
			 * @brief Virtual method declarations for this class. Contains all methods callable on
			 * this type, including inherited ones. Maps method name to its type.
			 * @note Unimplemented methods *do* exist in this map, but do not exist in the vtable.
			 */
			base::HashMap<base::StrID, ValidTypeID> available_methods;

			/**
			 * @brief A map from virtual method name to the name of the function that implements it.
			 * Contains all the implementations of virtual methods for this class/interface.
			 * Unimplemented methods do not exist in the vtable.
			 */
			base::HashMap<base::StrID, base::StrID> vtable;

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
		 * If declared as a class contains a vtable field at the front of field vector.
		 */
		struct Structure final {
			/**
			 * @brief Vector of fields. Order matters, as it determines field offsets.
			 * If this type is a class, the first field is always vtable pointer. Interfaces do not
			 * have any fields.
			 * @note In case a class extends another class, fields of the superclass are inserted
			 * into the vector right before the fields of the subclass.
			 */
			ObjIdNameMap<Field, Field::ID> fields;

			/**
			 * @brief Inheritance metadata for this structure type.
			 * @note Only classes and interfaces have this metadata.
			 */
			base::Optional<InheritanceMetadata> inheritance_metadata;

			// private:
			// 	friend class Type;

			// 	struct DefinitionData {};

			// 	/**
			// 	 * @brief During type definition, we need to forward some data for inheritance
			// metadata
			// 	 * construction during finalization. This field is used for that.
			// 	 */
			// 	base::Optional<DefinitionData> forwarded_definition_data;
		};

		/**
		 * @brief Variant type representation.
		 */
		struct Variant final {
			Bytes type_tag_size;
			std::vector<ValidTypeID>
				alternatives;  /// Order matters, as it determines type tag values.
		};

		/**
		 * @brief Function type representation.
		 */
		struct Function final {
			std::vector<ValidTypeID> parameters;
			ValidTypeID              result;
		};

		/**
		 * @brief Opaque type representation. This is used for types whose actual content is not
		 * known, like external types.
		 */
		struct Opaque final {
			Bytes size;
		};
	}

#define CONCRETE_TYPE_LIST                                                                    \
	concrete::Primitive, concrete::Pointer, concrete::FixedSizeTable, concrete::DynamicTable, \
		concrete::Structure, concrete::Variant, concrete::Function, concrete::Opaque

	template<class T>
	concept ConcreteType = base::IsOneOf<T, CONCRETE_TYPE_LIST>;

	using ConcreteTypeVariant = std::variant<std::monostate, CONCRETE_TYPE_LIST>;

}
