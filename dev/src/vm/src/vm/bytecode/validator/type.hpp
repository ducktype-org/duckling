#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include "vm/utils/stable_obj_id_name_map.hpp"
#include <vm/bytecode/type_of_data.hpp>

#include <cmath>
#include <unordered_set>
#include <variant>

namespace vm::code::type {
	// @TODO: Can this become a Ref<Type>?
	// @TODO: #1306 Maybe it can become Ref<Type>?
	using TypeID = usize;

	namespace concrete {
		struct Primitive {
			Bytes size;
		};

		struct Pointer {
			TypeID inner;

			Pointer(TypeID inner): inner(inner) {}
		};

		struct FixedSizeTable {
			TypeID inner;
			usize  element_count;
		};

		struct DynamicTable {
			TypeID inner;
		};

		/**
		 * @brief A field inside a Structure type.
		 */
		struct Field {
			STRONG_TYPEDEF_ID_DIRECT_CREATION(ID);
			base::StrID name;
			TypeID      type;
		};

		/**
		 * @brief Inheritance metadata for a Structure type. Contains information about superclasses
		 * and interfaces.
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
			 * @brief In a class-like way. Only one superclass allowed
			 */
			base::Optional<TypeID> extends;

			/**
			 * @brief In an interface-like way. Multiple interfaces allowed
			 */
			std::unordered_set<TypeID> implements;

			/**
			 * @brief Virtual method declarations for this class. Contains all methods callable on
			 * this type, including inherited ones. Maps method name to its type.
			 * @note Unimplemented methods *do* exist in this map, but do not exist in the vtable.
			 */
			base::HashMap<base::StrID, TypeID> virtual_methods;

			/**
			 * @brief A map from virtual method name to the name of the function that implements it.
			 * Contains all the implementations of virtual methods for this class/interface.
			 * Unimplemented methods do not exist in the vtable.
			 */
			base::HashMap<base::StrID, base::StrID> vtable;

			/**
			 * @brief Whether this type is abstract (i.e. cannot be instantiated)
			 */
			bool is_abstract = false;

			enum Kind { Class, Interface } kind = Kind::Class;
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

		struct Variant {
			Bytes               type_tag_size;
			std::vector<TypeID> alternatives;
		};

		struct Function {
			std::vector<TypeID> parameters;
			TypeID              result;
		};

		struct Opaque {
			Bytes size;
		};
	}

#define CONCRETE_TYPE_LIST                                                                    \
	concrete::Primitive, concrete::Pointer, concrete::FixedSizeTable, concrete::DynamicTable, \
		concrete::Structure, concrete::Variant, concrete::Function, concrete::Opaque

	template<class T>
	concept ConcreteType = base::IsOneOf<T, CONCRETE_TYPE_LIST>;

	/**
	 * @brief Representation of a type used for validation and lowering.
	 * @note We still don't know a size of a type, because pointer size is different between safe
	 * and fast modes. This also means that we cannot calculate field offsets for structured types
	 * yet, which is why they are not stored here.
	 * @note Type is first declared with just a name and an ID, then defined with its actual
	 * content, and then finalized. Finalization is needed to detect cyclic dependencies between
	 * types. During finalization we fill out some data like inheritance metadata for structures,
	 * because it's more effective.
	 * After finalization type is immutable. Type can be unfinalized.
	 */
	class Type {
		enum class State { Declared, Defined, Finalizing, Finalized } state = State::Declared;

	public:
		static Type declareType(base::StrID name, TypeID id) { return { name, id }; }

		void definePrimitive(Bytes size) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::Primitive{ size };
			if (name == "void") is_instantiable = false;
		}

		void definePointer(type::TypeID inner) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::Pointer{ inner };
			finalizeInstantiability();
		}

		void defineFixedSizeTable(type::TypeID inner, usize element_count) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::FixedSizeTable{ .inner = inner, .element_count = element_count };
			finalizeInstantiability();
		}

		void defineDynamicTable(type::TypeID inner) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::DynamicTable{ .inner = inner };
			finalizeInstantiability();
		}

		void defineData(const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			ObjIdNameMap<concrete::Field, concrete::Field::ID> fields;
			for (const auto& field_def: fields_definitions)
				fields.insert(
					concrete::Field{ .name = field_def.first, .type = field_def.second },
					field_def.first
				);

			concrete::Structure structure{
				.fields               = std::move(fields),
				.inheritance_metadata = {},
			};

			kind = structure;
			finalizeInstantiability();
		}

		void defineClass(
			const type::TypeID                                       vtable_type,
			const std::vector<std::pair<base::StrID, type::TypeID>>& fields_definitions,
			const bool                                               is_abstract,
			const base::Optional<type::TypeID>&                      extends,
			const std::vector<type::TypeID>&                         implements,
			const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
		) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			defineData(fields_definitions);
			CORE_ASSERT(
				std::holds_alternative<concrete::Structure>(kind),
				"Invalid type kind after defineData"
			);

			base::HashMap<base::StrID, type::TypeID> virtual_methods;
			for (const auto& method: new_virtual_methods)
				virtual_methods.put(method.first, method.second);

			base::HashMap<base::StrID, base::StrID> vtable;
			for (const auto& impl: implementations) vtable.put(impl.first, impl.second);

			// Forward the data to be used in inheritance metadata construction during finalization.
			std::get<concrete::Structure>(kind).inheritance_metadata
				= concrete::InheritanceMetadata{
					  .super_types     = { getId() },
					  .extends         = extends,
					  .implements      = implements | std::ranges::to<std::unordered_set>(),
					  .virtual_methods = virtual_methods,
					  .vtable          = vtable,
					  .is_abstract     = is_abstract,
					  .kind            = concrete::InheritanceMetadata::Kind::Class
				  };

			// Build inheritance metadata for this class.

			// // 1. Finalize superclass and interfaces first.
			// for (auto& i: implements) i->finalize();
			// if_opt_some(extends, superclass) superclass->finalize();

			// // 2. Fill the data
			// std::vector<std::pair<base::StrID, type::TypeID>> fields;
			// concrete::InheritanceMetadata                     imd
			// 	= { .super_types = { getId() },
			// 	    .extends     = {},
			// 	    .implements  = implements
			// 	                | std::views::transform([](auto& i) { return i->getId(); })
			// 	                | std::ranges::to<std::unordered_set>(),
			// 	    .virtual_methods = {},
			// 	    .vtable          = {},
			// 	    .is_abstract     = is_abstract,
			// 	    .kind            = concrete::InheritanceMetadata::Kind::Class };

			// match_optional(extends) {
			// 	opt_some(superclass) {
			// 		CORE_ASSERT(
			// 			superclass->is<concrete::Structure>(), "Superclass must be a structure"
			// 		);
			// 		const auto& super_structure = superclass->get<concrete::Structure>();
			// 		const auto& super_imd       = super_structure.inheritance_metadata.expect(
			//             "Superclass must have inheritance metadata"
			//         );

			// 		// Fields. Superclass fields are inserted before subclass fields.
			// 		for (const auto& field: superclass->get<concrete::Structure>().fields)
			// 			fields.emplace_back(field.name, field.type);
			// 		// Super types
			// 		imd.super_types.insert(
			// 			superclass->get<concrete::Structure>()
			// 				.inheritance_metadata->super_types.begin(),
			// 			superclass->get<concrete::Structure>().inheritance_metadata->super_types.end()
			// 		);
			// 		// Extends
			// 		imd.extends = superclass->getId();
			// 		// Virtual methods
			// 		for (const auto& method: super_imd.virtual_methods)
			// 			imd.virtual_methods.put(method.first, method.second);
			// 		// Vtable
			// 		for (const auto& impl: super_imd.vtable)
			// 			imd.vtable.put(impl.first, impl.second);
			// 	}

			// 	opt_none { fields.emplace_back(base::StrID(".vtable"), vtable_type->getId()); }
			// }


			// // 3. Add fields from this class and build inheritance metadata for this class.
			// // Fields
			// for (const auto& new_field: fields_definitions)
			// 	fields.emplace_back(new_field.first, new_field.second->getId());
			// // Virtual methods
			// for (const auto& new_virtual_method: new_virtual_methods)
			// 	imd.virtual_methods.put(
			// 		new_virtual_method.first, new_virtual_method.second->getId()
			// 	);
			// // Vtable
			// for (const auto& impl: implementations) imd.vtable.put(impl.first, impl.second);

			// // 4. Put the data into the type and finalize it.
			// defineData(fields_definitions);

			// CORE_ASSERT(
			// 	std::holds_alternative<concrete::Structure>(kind),
			// 	"Invalid type kind after defineData"
			// );
			// std::get<concrete::Structure>(kind).inheritance_metadata = imd;

			finalizeInstantiability();
		}

		void defineInterface(
			const std::vector<type::TypeID>&                         implements,
			const std::vector<std::pair<base::StrID, type::TypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>&  implementations
		) {
			throw base::NotYetImplemented("Interface types are not yet supported");
		}

		void defineVariant(const std::vector<TypeID>& variant_types) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			CORE_ASSERT(variant_types.size() != 0, "Cannot define variant with no alternatives");
			state = State::Defined;

			// log_256(x) = log_2(x) / log_2(256) = log_2(x) / 8.0
			const auto needed_bytes = ceil(log2(static_cast<double>(variant_types.size())) / 8.0);

			// Need to get a power of 2 - 1, 2, 4, 8, 16 etc
			// 2 ** (ceil(log2(needed_bytes)))
			const auto rounded_to_power_of_2
				= static_cast<usize>(std::pow(2, ceil(log2(needed_bytes))));

			kind = concrete::Variant{ .type_tag_size = Bytes(rounded_to_power_of_2),
				                      .alternatives  = variant_types };
		}

		void defineFunction(const std::vector<TypeID>& parameters, TypeID result) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::Function{ .parameters = parameters, .result = result };
			finalizeInstantiability();
		}

		void defineOpaque(Bytes size) {
			CORE_ASSERT(state == State::Declared, "Bad type define");
			state = State::Defined;

			kind = concrete::Opaque{ size };
			finalizeInstantiability();
		}

		/**
		 * @brief Finalize this type. During finalization we check for cyclic dependencies between
		 * types, and calculate some data that is needed for validation and lowering, like
		 * inheritance metadata for structures. After finalization type is immutable.
		 * @note There is no need for a type to be "unfinalizable", because we finalize all types at
		 * the end of validation, and after that we don't need to change them anymore.
		 */
		void finalize(ObjIdNameMap<type::Type>& types) {
			switch (state) {
			case State::Declared:
				CORE_PANIC("Tried to finalize a type that was not defined");
			case State::Defined:
				state = State::Finalizing;
				break;
			case State::Finalizing:
				CORE_PANIC("Cyclic dependency not detected during type validation");
			case State::Finalized:
				return;
			}
			variant_match(kind) {
				variant_case(concrete::Structure, structure) {
					// 1. Finalize superclass and interfaces first.
					for (auto& i: implements) i->finalize();
					if_opt_some(extends, superclass) superclass->finalize();

					// 2. Fill the data
					std::vector<std::pair<base::StrID, type::TypeID>> fields;
					concrete::InheritanceMetadata                     imd
						= { .super_types = { getId() },
						    .extends     = {},
						    .implements  = implements
						                | std::views::transform([](auto& i) { return i->getId(); })
						                | std::ranges::to<std::unordered_set>(),
						    .virtual_methods = {},
						    .vtable          = {},
						    .is_abstract     = is_abstract,
						    .kind            = concrete::InheritanceMetadata::Kind::Class };

					match_optional(extends) {
						opt_some(superclass) {
							CORE_ASSERT(
								superclass->is<concrete::Structure>(),
								"Superclass must be a structure"
							);
							const auto& super_structure = superclass->get<concrete::Structure>();
							const auto& super_imd = super_structure.inheritance_metadata.expect(
								"Superclass must have inheritance metadata"
							);

							// Fields. Superclass fields are inserted before subclass fields.
							for (const auto& field: superclass->get<concrete::Structure>().fields)
								fields.emplace_back(field.name, field.type);
							// Super types
							imd.super_types.insert(
								superclass->get<concrete::Structure>()
									.inheritance_metadata->super_types.begin(),
								superclass->get<concrete::Structure>()
									.inheritance_metadata->super_types.end()
							);
							// Extends
							imd.extends = superclass->getId();
							// Virtual methods
							for (const auto& method: super_imd.virtual_methods)
								imd.virtual_methods.put(method.first, method.second);
							// Vtable
							for (const auto& impl: super_imd.vtable)
								imd.vtable.put(impl.first, impl.second);
						}

						opt_none {
							fields.emplace_back(base::StrID(".vtable"), vtable_type->getId());
						}
					}


					// 3. Add fields from this class and build inheritance metadata for this class.
					// Fields
					for (const auto& new_field: fields_definitions)
						fields.emplace_back(new_field.first, new_field.second->getId());
					// Virtual methods
					for (const auto& new_virtual_method: new_virtual_methods)
						imd.virtual_methods.put(
							new_virtual_method.first, new_virtual_method.second->getId()
						);
					// Vtable
					for (const auto& impl: implementations) imd.vtable.put(impl.first, impl.second);
				}
			}
		}

		[[nodiscard]] base::StrID getName() const { return name; }

		[[nodiscard]] TypeID getId() const { return id; }

		template<ConcreteType T>
		[[nodiscard]]
		const T& get() const {
			return std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		const base::Optional<Ref<T>> maybeGet() const {
			if (!is<T>()) return {};
			return &std::get<T>(kind);
		}

		template<ConcreteType T>
		[[nodiscard]]
		bool is() const {
			return std::holds_alternative<T>(kind);
		}

		bool operator==(const Type& other) const { return other.id == id; }

		// /**
		//  * Get inner type of pointer, fixed size or dynamic table
		//  * @return Some(inner type) for pointer, fixed size or dynamic table. none otherwise
		//  */
		// base::Optional<TypeCRef> getInnerType() const;

		// [[nodiscard]]
		// bool isTriviallyCopyable() const;

		// // data
		// [[nodiscard]]
		// base::Optional<Bytes> getFieldOffsetByName(base::StrID field_name) const;
		// [[nodiscard]]
		// base::Optional<base::CRef<std::vector<kind::DataField>>> getFields() const;

		// // variant
		// base::Optional<usize>                 getTypeTagSizeBytes() const;
		// base::Optional<std::vector<TypeCRef>> getVariantAlternatives() const;

		// // inheritance
		// [[nodiscard]]
		// base::Optional<base::CRef<InheritanceMetadata>> getInheritanceMetadata() const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getSuperClass() const;
		// [[nodiscard]]
		// bool inheritsFrom(TypeCRef other) const;
		// [[nodiscard]]
		// bool isInstantiable() const;

		// // function
		// [[nodiscard]]
		// base::Optional<u64> getParameterCount() const;
		// [[nodiscard]]
		// base::Optional<u64> getParametersSize() const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getNthParameterType(u64 parameter_id) const;
		// [[nodiscard]]
		// base::Optional<TypeCRef> getResultType() const;

	private:
		void finalizeInstantiability() {
			throw base::NotYetImplemented("finalizeInstantiability is not implemented yet");
		}

		bool is_instantiable = true;

		Bytes size{ -1 };

		Type(base::StrID name, TypeID id): name(name), id(id) {}

		base::StrID name;
		TypeID      id;

		std::variant<std::monostate, CONCRETE_TYPE_LIST> kind;
	};

}
