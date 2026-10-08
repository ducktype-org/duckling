// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file errors.hpp
 * @brief Errors and error messages related to failed coercions, plus the reasons a coercion may
 * fail with.
 */
#pragma once

#include <helios_private/errors/dia_interactive_elements.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/box_or_ref.hpp>

#include <diagnostic/message.hpp>
#include <query_framework/context/context.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief The specific reason a coercion cannot be performed.
 */
MAKE_STRINGIFYABLE_ENUM(compiler::helios, uint8_t, InvalidCoercionReason,
		/// The source type is not coercible to the target type.
		IncompatibleTypes,
		/// The value's type is not copyable, but this coercion required a copy.
		TypeNotCopyable,
		/// The value is copyable but not trivially copyable. The implicit copy must be made explicit
		/// with `copy` or `move` keyword.
		RequiresExplicitCopyMove
);

namespace compiler::helios {
	class Coercion;

	class IncompatibleTypesError final: public dia::MessageWithCodeFragment {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "incompatible_types" };
		}

	public:
		IncompatibleTypesError(
			dia::StablePosition                 given_position,
			Box<InteractiveType>                actual_type,
			Box<InteractiveType>                expected_type,
			base::Optional<dia::StablePosition> coercion_expects_pos
		);
	};

	/**
	 * @brief Error logged when an expression cannot be coerced to any of the several accepted
	 * types. The error of every attempted coercion is attached to this message.
	 */
	class NoMatchingExpectedTypeError final: public dia::MessageWithCodeFragment {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_matching_expected_type" };
		}

	public:
		NoMatchingExpectedTypeError(
			dia::StablePosition given_position, Box<InteractiveType> actual_type
		);

		/**
		 * @brief Adds an explore link for a single accepted type, pointing to the error of the
		 * coercion that was attempted to this type.
		 */
		void addExploreAcceptedType(std::string accepted_type, Box<dia::MessageBase> coercion_error);
	};

	/**
	 * @brief Optional overrides of the default coercion failure errors.
	 */
	struct CoercionErrorOverrides final {
		using Logger = std::function<void(query::Context&)>;

		/// Override for `InvalidCoercionReason::IncompatibleTypes`.
		base::Optional<Logger> incompatible_types = {};
		/// Override for `InvalidCoercionReason::TypeNotCopyable`.
		base::Optional<Logger> type_not_copyable = {};
		/// Override for `InvalidCoercionReason::RequiresExplicitCopyMove`.
		base::Optional<Logger> requires_explicit_copy_move = {};
	};

	/**
	 * @brief Builds the default diagnostic message describing why a coercion failed, based on the
	 * `reason`.
	 */
	[[nodiscard]] Box<dia::MessageBase> getCoercionError(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos = {}
	);

	/**
	 * @brief Logs the coercion failure for `reason`. Uses the matching override in
	 * `error_overrides` if one is provided, otherwise logs the default message.
	 */
	void logCoercionFailure(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides              error_overrides = {}
	);

	/**
	 * @brief Logs the failure of coercing `source_symbol_type` to any of the `expected_types`.
	 * The error of every attempted coercion is attached to the logged message.
	 * @param failure_reasons Reason of the failure for every type of `expected_types`, in the same
	 * order. Must have the same size as `expected_types`.
	 */
	void logNoMatchingExpectedTypeFailure(
		query::Context&              ctx,
		const std::vector<Coercion>& failed_coercions,
		dia::StablePosition          source_position
	);
}
