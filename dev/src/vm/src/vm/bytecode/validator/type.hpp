#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/type_size.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <unordered_set>
#include <variant>

namespace vm::code::type {
	/**
	 * @brief TypeID is a unique identifier of a type. It is used to refer to types. It is an index
	 * in the TypeContext's type map.
	 * @note It's done this way to allow copying of types without worrying about pointer/reference
	 * references to types can be invalidated when the TypeContext is e.g. copied.
	 * @TODO: #1306 Maybe it can become Ref<Type>?
	 */
	using TypeID = usize;

	namespace concrete {
		/**
		 * @brief Primitive type representation.
		 */
		struct Primitive {
			Bytes size;
		};

		/**
		 * @brief Pointer type representation.
		 */
		struct Pointer {
			TypeID inner;

			Pointer(TypeID inner): inner(inner) {}
		};

		/**
		 * @brief Fixed-size table type representation.
		 */
		struct FixedSizeTable {
			TypeID inner;
			usize  element_count;
		};

		/**
		 * @brief Dynamic table type representation.
		 */
		struct DynamicTable {
			TypeID inner;
		};

		/**
		 * @brief A field inside a Structure type.
		 */
		struct Field {
			STRONG_TYPEDEF_ID_DIRECT_CREATION(ID);
			type::TypeSize offset;
			base::StrID    name;
			TypeID         type;
		};

		/**
		 * @brief Inheritance metadata for a Structure type. Contains information related to
		 * inheritance, like super types, implemented interfaces, virtual methods and vtable.
		 */
		struct InheritanceMetadata {
			/**
			 * @brief All the types this type directly or indirectly inherits from.
			 * This is cached for easier and faster lookup during validation and lowering.
			 * @note This includes the type itself, because a type is considered to inherit from
			 * itself.
			 * @note Both a class and an interface can be a super type.
			 */
			std::unordered_set<TypeID> super_types;

			/**
			 * @brief In an interface-like way. Multiple interfaces allowed
			 */
			std::unordered_set<TypeID> implements;

			/**
			 * @brief Virtual method declarations for this class. Contains all methods callable on
			 * this type, including inherited ones. Maps method name to its type.
			 * @note Unimplemented methods *do* exist in this map, but do not exist in the vtable.
			 */
			base::HashMap<base::StrID, TypeID> available_methods;

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
			struct ClassKind {
				/**
				 * @brief A super-class. Only one super-class allowed
				 */
				base::Optional<TypeID> extends;

				/**
				 * @brief Whether this type is abstract (i.e. cannot be instantiated)
				 */
				bool is_abstract = false;
			};

			/**
			 * @brief Interface kind. If this type is an interface, then no additional data is needed.
			 */
			struct InterfaceKind {};

			std::variant<InterfaceKind, ClassKind> kind;
		};

		/**
		 * @brief Struct/Class (data) type representation.
		 * If declared as a class contains a vtable field at the front of field vector.
		 */
		struct Structure {
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
		};

		/**
		 * @brief Variant type representation.
		 */
		struct Variant {
			Bytes               type_tag_size;
			std::vector<TypeID> alternatives;  /// Order matters, as it determines type tag values.
		};

		/**
		 * @brief Function type representation.
		 */
		struct Function {
			std::vector<TypeID> parameters;
			TypeID              result;
		};

		/**
		 * @brief Opaque type representation. This is used for types whose actual content is not
		 * known, like external types.
		 */
		struct Opaque {
			Bytes size;
		};
	}

#define CONCRETE_TYPE_LIST                                                                    \
	concrete::Primitive, concrete::Pointer, concrete::FixedSizeTable, concrete::DynamicTable, \
		concrete::Structure, concrete::Variant, concrete::Function, concrete::Opaque

	template<class T>
	concept ConcreteType = base::IsOneOf<T, CONCRETE_TYPE_LIST>;

	using ConcreteTypeVariant = std::variant<std::monostate, CONCRETE_TYPE_LIST>;

	/**
	 * @brief Representation of a type used for validation and lowering.
	 * @note We still don't know a size of a type, because pointer size is different between safe
	 * and fast modes. This also means that we cannot calculate field offsets for structured types
	 * yet, which is why they are not stored here.
	 * @note Type is first declared with just a name and an ID, then defined with its actual
	 * content, and then finalized. Finalization is needed to detect cyclic dependencies between
	 * types. During finalization we fill out some data like inheritance metadata for structures,
	 * because it's more effective.
	 * After finalization type is immutable. Type cannot be unfinalized.
	 */
	class Type {
		enum class State { Declared, Defined, Finalizing, Finalized } state = State::Declared;

	public:
		/****************/
		/* Constructors */
		/****************/
		static Type declareType(base::StrID name, TypeID id);

		void definePrimitive(Bytes size);

		void definePointer(type::TypeID inner);

		void defineFixedSizeTable(type::TypeID inner, usize element_count);

		void defineDynamicTable(type::TypeID inner);

		void defineData(const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions);

		void defineClass(
			const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions,
			const bool                                               is_abstract,
			const base::Optional<type::TypeID>&                      extends,
			const std::vector<type::TypeID>&                         implements,
			const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
		);

		void defineInterface(
			const std::vector<type::TypeID>&                         implements,
			const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
		);

		void defineVariant(const std::vector<TypeID>& variant_types);

		void defineFunction(const std::vector<TypeID>& parameters, TypeID result);

		void defineOpaque(Bytes size);

		/**
		 * @brief Finalize this type. During finalization we check for cyclic dependencies between
		 * types, and calculate some data that is needed for validation and lowering, like
		 * inheritance metadata for structures. After finalization type is immutable.
		 * @note There is no need for a type to be "unfinalizable", because we finalize all types at
		 * the end of validation, and after that we don't need to change them anymore.
		 */
		void finalize(ObjIdNameMap<type::Type>& types);

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

		[[nodiscard]] type::TypeSize getSize() const;

		bool operator==(const Type& other) const;

		bool operator==(const TypeID& other_id) const;

		[[nodiscard]] bool isPodType() const;

	private:
		void finalizeInstantiability(ObjIdNameMap<type::Type>& types);

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
		bool is_pod = true;

		type::TypeSize size = type::TypeSize(Bytes(0), 0);

		Type(base::StrID name, TypeID id);

		base::StrID name;
		TypeID      id;

		ConcreteTypeVariant kind;
	};

}
