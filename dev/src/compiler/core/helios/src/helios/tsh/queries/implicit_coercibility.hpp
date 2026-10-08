// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file implicit_coercibility.hpp
 * @brief Interface to deducing whether an implicit coercion of two values is allowed.
 *
 * An implicit coercion is when, for example, a boolean is expected, but
 * and integer is given. A desirable (and common) behaviour may be to
 * convert the integer value to true if and only if it is non-zero.
 *
 * Another context in which implicit coercions are desirable is when
 * casting from subclass to superclass.
 *
 * In other words, a coercion is the conversion of a value of one type, to a value of another
 * type, be it with a no-op (cast, if upwards a class hierarchy) or otherwise (conversion).
 *
 * These queries do not determine how to perform a coercion.
 * They only determine whether one should be considered.
 * A coercion may thus be allowed but not implemented; or implemented
 * but not allowed to be used implicitly by the compiler, so the user
 * may define a coercion from class A to class B, but not want it
 * to ever be used implicitly (in C++ that is achieved by annotating a
 * single-argument constructor with the `explicit` keyword).
 *
 * This is typically determined by rules specific for the Kind of the source type and
 * value categories (see: ValueCategory) of the source and target values.
 *
 * The user may also declare that they desire an implicit coercion to be considered.
 */

#pragma once

#include "../abstract_type.hpp"
#include "../expression_type.hpp"

#include <base/collections/maps.hpp>

#include <hashing/hash.hpp>
#include <query_framework/query_int.hpp>

// In the future, coercibility could work significantly differently.
// For example, these functions could also return the OperationID of the coercion operation.
// The current coercion implementation has not yet been tested.
// @TODO: Consider the above and add tests

namespace compiler::tsh {
	/**
	 * @brief Key for QueryImplicitCoercibilityOnInfo.
	 */
	struct KeyFor_QueryImplicitCoercibilityOnAbstractType final {
		/**
		 * @brief Source type of the coercion.
		 */
		AbstractType source;

		/**
		 * @brief Target type of the coercion.
		 */
		AbstractType target;

		KeyFor_QueryImplicitCoercibilityOnAbstractType(
			const AbstractType& source, const AbstractType& target
		):
			  source(source),
			  target(target) {}

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryImplicitCoercibilityOnAbstractType&) const
			= default;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(source, target);
		}
	};

	/**
	 * @brief Query to check whether implicit coercion from one type described by AbstractType to
	 * another is allowed.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryImplicitCoercibilityOnAbstractType,
		KeyFor_QueryImplicitCoercibilityOnAbstractType,
		bool,
		({ .uses_qresult = false })
	)

	/**
	 * @brief Key for QueryImplicitCoercibilityOnSymbolType.
	 */
	struct KeyFor_QueryImplicitCoercibilityOnSymbolType final {
		/**
		 * @brief Source value description of the coercion.
		 */
		SymbolType<> source;

		/**
		 * @brief Target value description of the coercion.
		 */
		SymbolType<> target;

		KeyFor_QueryImplicitCoercibilityOnSymbolType(
			const SymbolType<>& source, const SymbolType<>& target
		):
			  source(source),
			  target(target) {}

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryImplicitCoercibilityOnSymbolType&) const
			= default;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(source, target);
		}
	};

	/**
	 * @brief Query to check whether implicit coercion from one value described by SymbolType to
	 * another is allowed.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryImplicitCoercibilityOnSymbolType,
		KeyFor_QueryImplicitCoercibilityOnSymbolType,
		bool,
		({ .uses_qresult = false })
	)

	/**
	 * @brief Key for QueryImplicitCoercibilityOnExpressionType.
	 */
	struct KeyFor_QueryImplicitCoercibilityOnExpressionType final {
		/**
		 * @brief Source value description of the coercion.
		 */
		ExpressionType<> source;

		/**
		 * @brief Target value description of the coercion.
		 */
		ExpressionType<> target;

		KeyFor_QueryImplicitCoercibilityOnExpressionType(
			const ExpressionType<>& source, const ExpressionType<>& target
		):
			  source(source),
			  target(target) {}

		[[nodiscard]]
		auto operator<=>(const KeyFor_QueryImplicitCoercibilityOnExpressionType&) const
			= default;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(source, target);
		}
	};

	/**
	 * @brief Query to check whether implicit coercion from one value described by ExpressionType to
	 * another is allowed.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryImplicitCoercibilityOnExpressionType,
		KeyFor_QueryImplicitCoercibilityOnExpressionType,
		bool,
		({ .uses_qresult = false })
	)
}
