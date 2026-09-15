#pragma once

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/validator/valid_type/defined_kinds.hpp>
#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>
#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>

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
	class ValidType final {
	public:
		/****************/
		/* Constructors */
		/****************/
		static ValidType declareType(base::StrID name, ValidTypeID id);

		void definePrimitive(Bytes size);

		void definePointer(ValidTypeID inner);

		/**
		 * @brief Defines a C pointer: a raw 8-byte native address. An absent inner means an
		 * unknown pointee (C's `void*`).
		 */
		void defineCPointer(const base::Optional<ValidTypeID>& inner);

		void defineFixedSizeTable(ValidTypeID inner, usize element_count);

		void defineDynamicTable(ValidTypeID inner);

		void defineData(
			const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions, bool packed
		);

		void defineClass(
			const std::vector<std::pair<base::StrID, ValidTypeID>>& fields_definitions,
			bool                                                    is_abstract,
			const base::Optional<ValidTypeID>&                      extends,
			const std::vector<ValidTypeID>&                         implements,
			const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>& implementations
		);

		void defineInterface(
			const std::vector<ValidTypeID>&                         implements,
			const std::vector<std::pair<base::StrID, ValidTypeID>>& new_virtual_methods,
			const std::vector<std::pair<base::StrID, base::StrID>>& implementations
		);

		void defineVariant(const std::vector<ValidTypeID>& variant_types);

		void defineFunction(
			const std::vector<ValidTypeID>& parameters, const std::vector<ValidTypeID>& result
		);

		void defineOpaque(Bytes size);

		/**
		 * @brief Finalize this type. During finalization, we check for cyclic dependencies between
		 * types, and calculate some data that is needed for validation and lowering, like
		 * inheritance metadata for structures. After finalization type is immutable.
		 * @note There is no need for a type to be "unfinalizable", because we finalize all types at
		 * the end of validation, and after that we don't need to change them anymore.
		 */
		void finalize(ValidTypeMap& types);

		/**********************/
		/* General operations */
		/**********************/

		[[nodiscard]] base::StrID getName() const;

		[[nodiscard]] ValidTypeID getID() const;

		template<ConcreteType T>
		[[nodiscard]]
		base::Optional<CRef<T>> maybeGetKindAs() const {
			variant_match(state) {
				variant_case(ValidType::Finalized, finalized) {
					if (!isKind<T>()) return {};
					return &std::get<T>(finalized.kind);
				}
				variant_default { CORE_PANIC("Tried to get kind of a type that is not finalized"); }
			}
		}

		template<ConcreteType T>
		[[nodiscard]]
		CRef<T> getKindAs() const {
			return maybeGetKindAs<T>().expect("Tried to get kind of a type as the wrong type");
		}

		template<ConcreteType T>
		[[nodiscard]]
		bool isKind() const {
			variant_match(state) {
				variant_case(ValidType::Finalized, finalized) {
					return std::holds_alternative<T>(finalized.kind);
				}
				variant_default {
					CORE_PANIC("Tried to check kind of a type that is not finalized");
				}
			}
		}

		[[nodiscard]] FinalizedTypeVariant getKind() const;

		[[nodiscard]] bool isInstantiable() const;

		[[nodiscard]] TypeSize getSize() const;

		/**
		 * @brief Alignment requirement of this type, as a dual-width value (like size, alignment
		 * can differ between the 8- and 16-byte pointer modes). Non-packed structures align each
		 * field to the field type's alignment and round their total size up to the structure's
		 * alignment (the C layout rules). VM-only kinds (variants, dynamic tables) use alignment
		 * 1, as they are accessed via memcpy and never cross the FFI boundary.
		 */
		[[nodiscard]] TypeSize getAlignment() const;

		bool operator==(const ValidType& other) const;

		bool operator==(const ValidTypeID& other_id) const;

		/**
		 * @brief Whether this type is trivially copyable/POD(plain old data). This is true for
		 * types that can be copied with a simple memory copy, like primitives, opaques and
		 * fixed-size tables of trivially copyable types.
		 * This also means, that if a type requires maintaining block structure in the "Safe"
		 * mode, it is not trivially copyable.
		 */
		[[nodiscard]] bool isTriviallyCopyable() const;

		/**
		 * @brief Whether this type can cross the FFI boundary. FFI-compliant types are: primitives
		 * of size 1, 2, 4 or 8 (`f32`/`f64` must have their exact C sizes), C pointers,
		 * fixed-size tables of FFI-compliant types, and non-packed plain data structures (no
		 * classes or interfaces) whose every field is FFI-compliant.
		 * @note FFICompliant != TriviallyCopyable
		 */
		[[nodiscard]] bool isFFICompliant() const;

	private:
		/**
		 * @brief Helper function for finalize. Sets is_instantiable.
		 */
		void finalizeInstantiability(ValidTypeMap& types);

		/**
		 * @brief Helper function for finalize. Fills inheritance metadata for structures.
		 */
		finalized::Structure finalizeStructureData(
			ValidTypeMap& types, const defined::DefinedStructure& structure
		) const;

		/**
		 * @brief Whether this type is instantiable. This is false for types that cannot be
		 * instantiated, like void and dynamic tables or abstract classes. This flag is true for
		 * types that can be instantiated, like primitives, structures without un-instantiable
		 * fields, etc.
		 */
		bool is_instantiable = true;

		/**
		 * @brief For more information read docs of `isTriviallyCopyable`.
		 */
		bool is_trivially_copyable = true;

		/**
		 * @brief For more information read docs of `isFFICompliant`.
		 */
		bool is_ffi_compliant = false;

		TypeSize size = TypeSize(Bytes(0), 0);

		TypeSize alignment = TypeSize(Bytes(1), Bytes(1));

		ValidType(base::StrID name, ValidTypeID id);

		base::StrID name;
		ValidTypeID id;

		struct Declared final {};

		struct Defined final {
			DefinedTypeVariant kind;
		};

		struct Finalizing final {
			DefinedTypeVariant kind;
		};

		struct Finalized final {
			FinalizedTypeVariant kind;
		};

		std::variant<Declared, Defined, Finalizing, Finalized> state = Declared{};
	};

}
