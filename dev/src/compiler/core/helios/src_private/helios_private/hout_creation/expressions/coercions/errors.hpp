// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file errors.hpp
 * @brief Errors and error messages related to failed coercions.
 *
 * @TODO: #3732 Make the errors gooood.
 */
#pragma once

#include <helios/tsh/coercions/coercion.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <diagnostic/message.hpp>
#include <query_framework/context/context.hpp>

#include <functional>
#include <string>
#include <vector>

namespace compiler::helios::coercions {
	using tsh::coercions::Coercion;

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
		using MessageReturner = std::function<Box<dia::MessageBase>()>;

		/// Override for `tsh::coercions::coercion_error::IncompatibleTypes`.
		base::Optional<MessageReturner> incompatible_types = {};
		/// Override for `tsh::coercions::coercion_error::MutabilityMismatch`.
		base::Optional<MessageReturner> mutability_mismatch = {};
		/// Override for `tsh::coercions::coercion_error::NotImplemented`.
		base::Optional<MessageReturner> not_implemented = {};
		/// Override for `tsh::coercions::coercion_error::AmbiguousCoercion`.
		base::Optional<MessageReturner> ambiguous_coercion = {};
		/// Override for `tsh::coercions::coercion_error::TypeNotCopyable`.
		base::Optional<MessageReturner> type_not_copyable = {};
		/// Override for `tsh::coercions::coercion_error::RequiresExplicitCopyMove`.
		base::Optional<MessageReturner> requires_explicit_copy_move = {};

		/**
		 * @brief The override for a coercion refused for @p reason.
		 */
		[[nodiscard]] const base::Optional<MessageReturner>& overrideFor(
			const tsh::coercions::CoercionFailure& reason
		) const;
	};

	/**
	 * @brief Builds the diagnostic message describing why a coercion was refused. Uses the
	 * overrides provided in @p error_overrides if ones are provided.
	 */
	[[nodiscard]] Box<dia::MessageBase> getCoercionError(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos = {},
		const CoercionErrorOverrides&       error_overrides      = {}
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
		const CoercionErrorOverrides&       error_overrides = {}
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
