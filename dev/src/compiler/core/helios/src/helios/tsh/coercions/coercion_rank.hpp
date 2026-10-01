/**
 * @file coercion_rank.hpp
 * @brief How good of a match a coercion is.
 *
 * A rank is what candidates are compared against when performing function call overloads or
 * determining in into which value to pack a value when creating a variant type.
 *
 * For composite types, a complex coercion rank is the worst thing that it does.
 */

#pragma once

#include <cstdint>

namespace compiler::tsh::coercions {
	/**
	 * @brief The worst thing a coercion does. Ranked from the best to the worst.
	 */
	enum class Rank : uint8_t {
		/// Nothing happens to the value.
		Identity = 0,
		/// Only the mutability the value is seen with changes. Nothing happens to the value itself.
		MutabilityChange,
		/// The pointee is read out of a `ref`/`box`.
		Deref,
		/// A widening conversion(i.e. `i32` -> `i64`)
		Numeric,
		/// The value is packed into a variant. TODOP: Think where to place it. Maybe it should be
		/// just a little worse then mutability relax?
		VariantPack,
		/// The value is interpreted as a type.
		LiftToType,
		/// A user defined conversion that builds a new value out of the old one.
		/// @TODO: #3656 Nothing produces this yet.
		UserConversion,
		/// The value is compared against zero, which turns it into a boolean.
		ZeroCheck,
		/// The value is reinterpreted as a void type.
		RetypeVoid,
	};

	/**
	 * @brief How good of a match a coercion is, which is the `Rank` of the worst thing
	 * it does to the value.
	 */
	class CoercionRank final {
	public:
		/**
		 * @brief Nothing happens to the value.
		 */
		[[nodiscard]]
		static CoercionRank identity() {
			return {};
		}

		/**
		 * @brief Creates a rank with @p category.
		 */
		[[nodiscard]]
		static CoercionRank of(const Rank category) {
			return CoercionRank{ category };
		}

		/**
		 * @brief The rank of doing both things in a coercion. A coercion is as good as the worst
		 * thing it does.
		 */
		[[nodiscard]]
		static CoercionRank combine(const CoercionRank& left, const CoercionRank& right) {
			return CoercionRank{ left.rank > right.rank ? left.rank : right.rank };
		}

		[[nodiscard]]
		Rank getRank() const noexcept {
			return rank;
		}

		/**
		 * @brief Whether nothing at all happens to the value.
		 */
		[[nodiscard]]
		bool isIdentity() const noexcept {
			return rank == Rank::Identity;
		}

		/**
		 * @brief Whether this rank is a better match than @p other.
		 */
		[[nodiscard]]
		bool dominates(const CoercionRank& other) const noexcept {
			return rank < other.rank;
		}

		/**
		 * @brief Whether neither rank is a better than the other.
		 */
		[[nodiscard]]
		bool ties(const CoercionRank& other) const noexcept {
			return rank == other.rank;
		}

	private:
		CoercionRank() = default;

		explicit CoercionRank(const Rank category): rank(category) {}

		Rank rank{ Rank::Identity };
	};
}
