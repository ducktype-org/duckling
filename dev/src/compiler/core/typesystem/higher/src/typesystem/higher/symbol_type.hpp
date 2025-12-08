#pragma once

#include "abstract_type.hpp"
#include "mutability.hpp"

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
		 * @brief Determines weather the symbol has a trivial destructor.
		 *
		 * It is needed to determine if createing a lifetime flag is needed during LIR lowering.
		 *
		 * @return true if the symbol has a trivial destructor, false otherwise.
		 */
		[[nodiscard]]
		bool hasNoOpDestructor() const {
			if (reference_kind == ReferenceKind::Ref) {
				// Ref types have trivial destructors, because it do not own its contents.
				return true;
			}
			if (abstract_type.hasNoOpDestructor()) return true;
			// @TODO #1271: add more cases where destructor is trivial
			// NOTE: abstract_type check should probably be the last one as it may be expensive
			return false;
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
		u64 queryUnstablePerfectHash() const {
			static base::Map<SymbolType, u64> hashes{};
			if (const auto iter = hashes.find(*this); iter != hashes.end()) return iter->second;
			auto new_hash = hashes.size();
			hashes.put(*this, new_hash);
			return new_hash;
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
}
