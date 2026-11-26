// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "value_category.hpp"
#include "value_type.hpp"

#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

#include <compare>
#include <string>

namespace compiler::tsh {
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
	 * @brief The type of a declared symbol: the ValueType of the value it holds and the additional
	 * symbol properties like PlaceAccess, Uniqueness, Leakage.
	 *
	 * It only describes declarations which are strictly bound to a symbol, like variables,
	 * parameters, fields, for iterators and match bindings. It is the result of QueryTypeOfSymbol
	 * and the type stored for generated variables. Use ValueType everywhere else.
	 */
	class SymbolType final {
	public:
		SymbolType(
			ValueType<>       value_type,
			const PlaceAccess place_access,
			const Leakage     leakage    = Leakage::NonLeaking,
			const Uniqueness  uniqueness = Uniqueness::NonUnique
		):
			  value_type(value_type),
			  place_access(place_access),
			  leakage(leakage),
			  uniqueness(uniqueness) {}

		/**
		 * @brief The type of a symbol whose place may be written to, like one declared with `var`.
		 * @param value_type The type of the value held by the symbol.
		 */
		static SymbolType var(ValueType<> value_type) {
			return { value_type, PlaceAccess::Mutable };
		}

		/**
		 * @brief The type of a symbol whose place may only be read, like one declared with `let`.
		 * @param value_type The type of the value held by the symbol.
		 */
		static SymbolType let(ValueType<> value_type) {
			return { value_type, PlaceAccess::Immutable };
		}

		/**
		 * @brief The type of the value held by the symbol.
		 */
		[[nodiscard]]
		const ValueType<>& getValueType() const {
			return value_type;
		}

		/**
		 * @brief Whether the place of the symbol may be written to.
		 */
		[[nodiscard]]
		PlaceAccess getPlaceAccess() const {
			return place_access;
		}

		/**
		 * @brief Whether the value of the symbol may leak outside the defining scope.
		 */
		[[nodiscard]]
		Leakage getLeakage() const {
			return leakage;
		}

		/**
		 * @brief Whether other references to the value of the symbol may exist.
		 */
		[[nodiscard]]
		Uniqueness getUniqueness() const {
			return uniqueness;
		}

		auto operator<=>(const SymbolType& other) const = default;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(value_type, place_access, leakage, uniqueness);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const SymbolType& t
		) noexcept {
			addToHash(h, t.queryUnstablePerfectHash());
		}

		/**
		 * @brief A human readable description: the symbol properties followed by the value type,
		 * like `unique let i64`.
		 */
		[[nodiscard]]
		std::string toString() const {
			return base::strConcat(
				uniqueness == Uniqueness::Unique ? "unique " : "",
				leakage == Leakage::Leaking ? "leaking " : "",
				place_access == PlaceAccess::Immutable ? "let " : "",
				value_type.toString()
			);
		}

	private:
		ValueType<> value_type;    ///< The type of the value held by the symbol.
		PlaceAccess place_access;  ///< Whether the place of the symbol may be written to.
		Leakage     leakage;       ///< Whether the value may leak outside the defining scope.
		Uniqueness  uniqueness;    ///< Whether other references to the value may exist.
	};
}
