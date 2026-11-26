// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "abstract_type.hpp"
#include "view_access.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

namespace compiler::tsh {
	/**
	 * @brief The kind of Reference type. See documentation of each kind for details.
	 */
	enum class ReferenceKind : uint8_t {
		/**
		 * @brief A reference of this kind is a non-reference. It is the value taken directly.
		 */
		Direct,

		/**
		 * @brief A reference of this kind owns its referee.
		 *
		 * When the reference is destroyed, so is the referee.
		 * The reference does not give access to explicit destruction of the referee, because
		 * destruction is automatic.
		 */
		Box,

		/**
		 * @brief A reference of this kind specifically does **not** own its referee.
		 *
		 * When the reference is destroyed, the referee remains untouched.
		 * Additionally, the reference does not give access to explicit destruction of the referee.
		 * This kind of reference can only be constructed from a DIRECT or BOX reference.
		 */
		Ref,
	};

	/**
	 * @brief The ValueType class describes how a value is held, which is an AbstractType expanded
	 * with information about referencing.
	 *
	 * A ValueType<AbstractType> object will contain information about any type,
	 * while a ValueType<IntegralAbstractType> object is guaranteed to contain information
	 * about some Integral type described with an IntegralAbstractType object.
	 *
	 * It types values everywhere: symbols, expressions and the components of composite types,
	 * like tuples and variants. This represents a pure type looking from the user level.
	 * There are two possible expansions of the ValueType:
	 * - `SymbolType` - an object which describes a declared symbol. It includes other symbol
	 * properties different from the type itself. This includes PlaceAccess, Uniqueness and Leakage.
	 * - `ExpressionType` - an object which describes an expression. It includes other expression
	 * properties - mainly `ValueCategory`.`
	 *
	 * @tparam ABSTRACT_TYPE The underlying class from the AbstractType hierarchy.
	 */
	template<std::derived_from<AbstractType> ABSTRACT_TYPE = AbstractType>
	class ValueType final {
	public:
		/**
		 * @brief Constructs the ValueType from another ValueType.
		 *
		 * The source ValueType must hold an abstract type which is dynamically convertible
		 * to the abstract type expected by the target ValueType. Otherwise,
		 * the dynamic cast, and then the constructor, will fail.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source abstract type,
		 * from the AbstractType hierarchy.
		 * @param other The source ValueType.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		ValueType(const ValueType<OTHER_ABSTRACT_TYPE>& other):
			  abstract_type(other.getType()),
			  reference_kind(other.getRefKind()),
			  view_access(other.getViewAccess()) {}

		/**
		 * @brief Creates a Direct ValueType from an AbstractType.
		 *
		 * @param abstract_type The source abstract type, from the AbstractType hierarchy.
		 * @return A Direct ValueType<ABSTRACT_TYPE>.
		 */
		static ValueType<ABSTRACT_TYPE> direct(const ABSTRACT_TYPE abstract_type) {
			return ValueType<ABSTRACT_TYPE>(abstract_type, ReferenceKind::Direct);
		}

		/**
		 * @brief Creates a Ref ValueType from an AbstractType.
		 *
		 * @param abstract_type The source abstract type, from the AbstractType hierarchy.
		 * @param view_access Whether the place may be written through the reference.
		 * @return A Ref ValueType<ABSTRACT_TYPE>.
		 */
		static ValueType<ABSTRACT_TYPE> ref(
			const ABSTRACT_TYPE abstract_type, const ViewAccess view_access
		) {
			return ValueType<ABSTRACT_TYPE>(abstract_type, ReferenceKind::Ref, view_access);
		}

		/**
		 * @brief Creates a Box ValueType, from an AbstractType.
		 *
		 * @param abstract_type The source abstract type, from the AbstractType hierarchy.
		 * @return A Box ValueType<ABSTRACT_TYPE>.
		 */
		static ValueType<ABSTRACT_TYPE> box(const ABSTRACT_TYPE abstract_type) {
			return ValueType<ABSTRACT_TYPE>(abstract_type, ReferenceKind::Box);
		}

		/**
		 * @brief Gets the underlying abstract type.
		 * @return The underlying abstract type.
		 */
		[[nodiscard]]
		ABSTRACT_TYPE getType() const {
			return abstract_type;
		}

		/**
		 * @brief Gets the reference kind.
		 * @return The reference kind.
		 */
		[[nodiscard]]
		ReferenceKind getRefKind() const {
			return reference_kind;
		}

		/**
		 * @brief Get the access rules of a reference.
		 * @return The view access of a Ref, nothing for Direct and Box.
		 */
		[[nodiscard]]
		base::Optional<ViewAccess> getViewAccess() const {
			return view_access;
		}

		/**
		 * @brief Determines weather the symbol has a trivial destructor i.e. destructor that does
		 * not perform any operations. Importantly, It is used in LIR lowering to determine if
		 * destructor calls and lifetime flag are needed.
		 *
		 * @return true if the symbol has a trivial destructor, false otherwise.
		 */
		[[nodiscard]]
		bool isTriviallyDestructible(query::Context& ctx) const {
			if (reference_kind == ReferenceKind::Ref) {
				// Ref types have trivial destructors, because it do not own its contents.
				return true;
			}
			if (reference_kind == ReferenceKind::Box) {
				// Box types don't have trivial destructors, as they have to deallocate the
				// memory.
				return false;
			}
			if (abstract_type.isTriviallyDestructible(ctx)) return true;
			return false;
		}

		/**
		 * @brief Determines weather the symbol has a default constructor. This is true for
		 * primitive types or classes/arrays that store default constructible types, but not true
		 * for types like `void`, references and boxes;
		 *
		 * @return true if the symbol has a default constructor, false otherwise.
		 */
		[[nodiscard]]
		bool isDefaultConstructible(query::Context& ctx) const {
			// References and boxes are not default constructible.
			if (reference_kind != ReferenceKind::Direct) return false;
			return abstract_type.isDefaultConstructible(ctx);
		}

		/**
		 * @brief Determines weather the type has a trivial zero constructor, meaning it can be
		 * safely zero initialized and doesn't need a specially generated default constructor.
		 * This is true for primitive types, strings, lists and static arrays storing other
		 * trivially zero initializable types, but also classes with all of their fields being zero
		 * initializable and every one of them not having an initial value. For example:
		 * - `class T { a: i64 = 1; }` - this is not trivially zero initializable
		 * - `class U { b: i64; }` - this is trivially zero initializable
		 * - `class V { t: T; }` - this is not trivially zero initializable cause it's field type
		 * isn't.
		 * - `class V { u: U; }` - this is trivially zero initializable cause `U.b` doesn't have an
		 * initial value.
		 *
		 * @return true if the type can be default initialized by zeros, false otherwise
		 */
		[[nodiscard]]
		bool isTriviallyZeroInitializable(query::Context& ctx) const {
			// References and boxes are not default constructible.
			if (reference_kind != ReferenceKind::Direct) return false;
			return abstract_type.isTriviallyZeroInitializable(ctx);
		}

		/**
		 * @brief Checks if a value of this symbol can be copied.
		 *
		 * - Direct values are copyable if their underlying abstract type is copyable.
		 * - References are always copyable (the reference itself is copied).
		 * - Boxes are copyable if their underlying abstract type is copyable (implies a deep copy).
		 * @return True if the symbol is copyable, false otherwise.
		 */
		[[nodiscard]]
		bool isCopyable(query::Context& ctx) const {
			// References are always copyable, just a pointer copy.
			if (reference_kind == ReferenceKind::Ref) return true;
			// Box is copyable if the inner abstract type is. Although it requires a deep copy.
			return abstract_type.isCopyable(ctx);
		}

		/**
		 * @brief Checks if a value of this symbol can be copied trivially by just copying the
		 * values bytes.
		 *
		 * - Direct values are trivially copyable if their underlying abstract type is.
		 * - References are trivially copyable.
		 * - Boxes are never trivially copyable as they require heap allocation and a deep copy.
		 * @return True if the symbol is trivially copyable, false otherwise.
		 */
		[[nodiscard]]
		bool isTriviallyCopyable(query::Context& ctx) const {
			if (reference_kind == ReferenceKind::Ref) return true;
			if (reference_kind == ReferenceKind::Box) return false;
			return abstract_type.isTriviallyCopyable(ctx);
		}

		/**
		 * @brief Copies this ValueType with the abstract type replaced. The reference kind and the
		 * view access stay the same.
		 * @note Used by creating static array types in comp time.
		 * @param new_abstract_type The abstract type of the result.
		 * @return This ValueType with the given abstract type.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		ValueType<OTHER_ABSTRACT_TYPE> withType(const OTHER_ABSTRACT_TYPE new_abstract_type) const {
			return ValueType<OTHER_ABSTRACT_TYPE>(new_abstract_type, reference_kind, view_access);
		}

		/**
		 * @brief Copies this ValueType with the reference kind set to Direct.
		 * @return This ValueType as a Direct (non-reference) value.
		 */
		[[nodiscard]]
		ValueType toDirect() const {
			return ValueType(abstract_type, ReferenceKind::Direct);
		}

		/**
		 * @brief Copies this ValueType with the reference kind set to Box.
		 * @return This ValueType as an owning Box reference.
		 */
		[[nodiscard]]
		ValueType toBox() const {
			return ValueType(abstract_type, ReferenceKind::Box);
		}

		/**
		 * @brief Copies this ValueType with the reference kind set to Ref.
		 *
		 * When referencing a reference the resulting mutability is the weaker of the two.
		 * - `ref ref T -> ref T`,
		 * - `ref mut ref T -> ref T`,
		 * - `ref ref mut T -> ref T`,
		 * - `ref mut ref mut T -> ref mut T`.`
		 *
		 * @param new_view_access The view access of the outer reference.
		 * @return This ValueType as a ref with the appropriate ViewAccess.
		 */
		[[nodiscard]]
		ValueType toRef(const ViewAccess new_view_access) const {
			const bool writable
				= new_view_access == ViewAccess::ReadWrite
			  and view_access.copyValueOr(ViewAccess::ReadWrite) == ViewAccess::ReadWrite;
			return ValueType(
				abstract_type,
				ReferenceKind::Ref,
				writable ? ViewAccess::ReadWrite : ViewAccess::ReadOnly
			);
		}

		/**
		 * @brief Utility to get the pointee type from the current ValueType.
		 * Works both with reference RefKind and pointer abstract types.
		 * The RefKind of this symbol is considered first and the abstract type is only considered
		 * if the RefKind is Direct.
		 */
		[[nodiscard]]
		ValueType getPointeeValueType() const;

		/**
		 * @brief Three-way comparison with another ValueType.
		 *
		 * This comparison is arbitrary, and just as is the case with AbstractType,
		 * should only be used to index ordered data structures or compare for equality.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other ValueType.
		 * @return Result of comparison, as std::strong_ordering.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		auto operator<=>(const ValueType<OTHER_ABSTRACT_TYPE>& other) const {
			if (auto type_cmp = abstract_type <=> other.getType(); type_cmp != 0) return type_cmp;
			if (auto ref_cmp = reference_kind <=> other.getRefKind(); ref_cmp != 0) return ref_cmp;
			return view_access <=> other.getViewAccess();
		}

		/**
		 * @brief Template equality operator for ease of use, because a template <=> operator
		 * does not work very well for deducing other comparison operators.
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other ValueType.
		 * @return Result of comparison, as bool.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		bool operator==(const ValueType<OTHER_ABSTRACT_TYPE>& other) const {
			return *this <=> other == 0;
		}

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				abstract_type,
				reference_kind,
				view_access.has_value(),
				view_access.copyValueOr(ViewAccess::ReadOnly)
			);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const ValueType& t
		) noexcept {
			addToHash(h, t.queryUnstablePerfectHash());
		}

		[[nodiscard]]
		std::string toString() const {
			switch (reference_kind) {
			case ReferenceKind::Direct:
				return abstract_type.toString();
			case ReferenceKind::Box:
				return base::strConcat("box ", abstract_type.toString());
			case ReferenceKind::Ref:
				return base::strConcat(
					viewOperator(lang_def::Keyword::Ref, view_access.value()),
					abstract_type.toString()
				);
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Returns a source-oriented representation of this ValueType.
		 *
		 * The returned representation includes the reference kind, `ref`, `ref mut` or `box`, in the
		 * same way as @ref toString. The underlying type follows Duckling source syntax whenever it
		 * has a type-expression spelling and otherwise uses the human-readable fallback documented
		 * by AbstractType::sourceTypeName. The result is intended for user-facing output, subject
		 * to the name-qualification limitations documented by AbstractType::sourceTypeName.
		 *
		 * @param ctx Query context used to resolve namespace-qualified class names and baked
		 * template arguments.
		 * @return A string containing the source-oriented representation of this ValueType.
		 */
		[[nodiscard]]
		std::string sourceTypeName(query::Context& ctx) const;

	private:
		ABSTRACT_TYPE              abstract_type;
		ReferenceKind              reference_kind;
		base::Optional<ViewAccess> view_access;  ///< Only a ref has it.

		/**
		 * @brief Constructs the ValueType directly from its contents. Use direct(), ref() or box()
		 * instead.
		 */
		ValueType(
			const ABSTRACT_TYPE              abstract_type,
			const ReferenceKind              reference_kind,
			const base::Optional<ViewAccess> view_access = {}
		):
			  abstract_type(abstract_type),
			  reference_kind(reference_kind),
			  view_access(view_access) {
			CORE_ASSERT(
				view_access.has_value() == (reference_kind == ReferenceKind::Ref),
				"Only a Ref ValueType has a view access"
			);
		}

		template<std::derived_from<AbstractType>>
		friend class ValueType;
	};

	extern template auto ValueType<AbstractType>::getPointeeValueType() const
		-> ValueType<AbstractType>;
	extern template std::string ValueType<AbstractType>::sourceTypeName(query::Context&) const;
}
