#pragma once

#include "abstract_type.hpp"
#include "mutability.hpp"

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

	enum class Leakage : bool {
		/**
		 * @brief The value can leak outside the defining scope.
		 */
		Leaking,

		/**
		 * @brief The value cannot leak outside the defining scope.
		 */
		NonLeaking,
	};

	enum class Uniqueness : bool {
		/**
		 * @brief The value is unique, no other references to it can exist.
		 */
		Unique,

		/**
		 * @brief The value is not unique, other references to it are permitted.
		 */
		NonUnique,
	};

	/**
	 * @brief The SymbolType class contains information about the type of a symbol,
	 * which is an AbstractType expanded with information about referencing and mutability.
	 *
	 * A SymbolType<AbstractType> object will contain information about any type,
	 * while a SymbolType<IntegralAbstractType> object is guaranteed to contain information
	 * about some Integral type described with an IntegralAbstractType object.
	 *
	 * It's called "SymbolType" because it is used to type symbols, like variables and parameters.
	 * However, it is also used as components of composite types, like tuples and variants.
	 * Note, that function and class types are also composite types, but in their case, the
	 * components are more obviously associated with symbols.
	 *
	 * @tparam ABSTRACT_TYPE The underlying class from the AbstractType hierarchy.
	 */
	template<std::derived_from<AbstractType> ABSTRACT_TYPE = AbstractType>
	class SymbolType {
	public:
		/**
		 * @brief Constructs the SymbolType from another SymbolType.
		 *
		 * The source SymbolType must hold an abstract type which is dynamically convertible
		 * to the abstract type expected by the target SymbolType. Otherwise,
		 * the dynamic cast, and then the constructor, will fail.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source abstract type,
		 * from the AbstractType hierarchy.
		 * @param other The source SymbolType.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		SymbolType(const SymbolType<OTHER_ABSTRACT_TYPE>& other):
			  abstract_type(other.getType()),
			  reference_kind(other.getRefKind()),
			  mutability(other.getMutability()),
			  leakage(other.getLeakage()),
			  uniqueness(other.getUniqueness()) {}

		/**
		 * @brief Constructs the SymbolType directly from its contents.
		 * @param abstract_type The source abstract type, from the AbstractType hierarchy.
		 * @param reference_kind The reference kind.
		 * @param mutability The mutability.
		 * @param leakage The leakage.
		 * @param uniqueness The uniqueness.
		 */
		SymbolType(
			const ABSTRACT_TYPE abstract_type,
			const ReferenceKind reference_kind,
			const Mutability    mutability,
			const Leakage       leakage    = Leakage::NonLeaking,
			const Uniqueness    uniqueness = Uniqueness::NonUnique
		):
			  abstract_type(abstract_type),
			  reference_kind(reference_kind),
			  mutability(mutability),
			  leakage(leakage),
			  uniqueness(uniqueness) {}

		// @TODO: #2348 Use this whenever abstract type is promoted to symbol type and change the
		// mutability defaults
		/**
		 * @brief Creates a SymbolType from an AbstractType with a set of default symbol properties.
		 *
		 * This static factory method makes a SylbolType<> that has Direct reference kind, is
		 * Mutable, NonLeaking and NonUnique and wrpas the given abstract type.
		 *
		 * @warning Do not wrap this method in "convenience" functions or implicit conversions.
		 * Hiding this call behind a shorter or automated wrapper defeats its purpose of making the
		 * transition from AbstractType to SymbolType explicit and conscious decision.
		 *
		 * @param abstract_type The source abstract type, from the AbstractType hierarchy.
		 * @return A SymbolType<ABSTRACT_TYPE> with default symbol properties.
		 */
		static SymbolType<ABSTRACT_TYPE> withDefaults(const ABSTRACT_TYPE abstract_type) {
			return SymbolType<ABSTRACT_TYPE>(
				abstract_type,
				ReferenceKind::Direct,
				Mutability::Mutable,
				Leakage::NonLeaking,
				Uniqueness::NonUnique
			);
		}

		/**
		 * @brief Same as above, but with immutability.
		 * @TODO: #2348 change the names to withDefaultsMut and withDefaults, rustlike
		 */
		static SymbolType<ABSTRACT_TYPE> withDefaultsConst(const ABSTRACT_TYPE abstract_type) {
			return SymbolType<ABSTRACT_TYPE>(
				abstract_type,
				ReferenceKind::Direct,
				Mutability::Immutable,
				Leakage::NonLeaking,
				Uniqueness::NonUnique
			);
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
		 * @brief Gets the mutability.
		 * @return The mutability.
		 */
		[[nodiscard]]
		Mutability getMutability() const {
			return mutability;
		}

		/**
		 * @brief Gets the leakage.
		 * @return The leakage.
		 */
		[[nodiscard]]
		Leakage getLeakage() const {
			return leakage;
		}

		/**
		 * @brief Gets the uniqueness.
		 * @return The uniqueness.
		 */
		[[nodiscard]]
		Uniqueness getUniqueness() const {
			return uniqueness;
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

		[[nodiscard]]
		SymbolType withReferenceKind(const ReferenceKind new_reference_kind) const {
			return SymbolType(abstract_type, new_reference_kind, mutability, leakage, uniqueness);
		}

		[[nodiscard]]
		SymbolType withMutability(const Mutability new_mutability) const {
			return SymbolType(abstract_type, reference_kind, new_mutability, leakage, uniqueness);
		}

		/**
		 * @brief Utility to get the pointee type from the current symbol type.
		 * Works both with reference RefKind and pointer abstract types.
		 * The RefKind of this symbol is considered first and the abstract type is only considered
		 * if the RefKind is Direct.
		 */
		[[nodiscard]]
		SymbolType getPointeeSymbolType() const;

		/**
		 * @brief Three-way comparison with another SymbolType.
		 *
		 * This comparison is arbitrary, and just as is the case with AbstractType,
		 * should only be used to index ordered data structures or compare for equality.
		 *
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other SymbolType.
		 * @return Result of comparison, as std::strong_ordering.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		auto operator<=>(const SymbolType<OTHER_ABSTRACT_TYPE>& other) const {
			if (auto type_cmp = abstract_type <=> other.getType(); type_cmp != 0) return type_cmp;
			if (auto ref_cmp = reference_kind <=> other.getRefKind(); ref_cmp != 0) return ref_cmp;
			if (auto mut_cmp = mutability <=> other.getMutability(); mut_cmp != 0) return mut_cmp;
			if (auto leak_cmp = leakage <=> other.getLeakage(); leak_cmp != 0) return leak_cmp;
			return uniqueness <=> other.getUniqueness();
		}

		/**
		 * @brief Template equality operator for ease of use, because a template <=> operator
		 * does not work very well for deducing other comparison operators.
		 * @tparam OTHER_ABSTRACT_TYPE The type of the source type description,
		 * from the AbstractType hierarchy.
		 * @param other The other SymbolType.
		 * @return Result of comparison, as bool.
		 */
		template<std::derived_from<AbstractType> OTHER_ABSTRACT_TYPE>
		[[nodiscard]]
		bool operator==(const SymbolType<OTHER_ABSTRACT_TYPE>& other) const {
			return *this <=> other == 0;
		}

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				abstract_type, reference_kind, mutability, leakage, uniqueness
			);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const SymbolType& t
		) noexcept {
			addToHash(h, t.queryUnstablePerfectHash());
		}

		[[nodiscard]]
		std::string toString() const {
			using enum ReferenceKind;
			return base::strConcat(
				uniqueness == Uniqueness::Unique ? "unique " : "",
				leakage == Leakage::Leaking ? "leaking " : "",
				mutability == Mutability::Mutable ? "" : "const ",
				reference_kind == Direct ? ""
				: reference_kind == Box  ? "box "
										 : "ref ",
				abstract_type.toString()
			);
		}

	private:
		ABSTRACT_TYPE abstract_type;
		ReferenceKind reference_kind;
		Mutability    mutability;
		Leakage       leakage;
		Uniqueness    uniqueness;
	};

	extern template auto SymbolType<AbstractType>::getPointeeSymbolType() const
		-> SymbolType<AbstractType>;
}
