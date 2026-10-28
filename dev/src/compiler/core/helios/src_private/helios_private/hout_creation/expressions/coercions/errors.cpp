// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "errors.hpp"

#include <helios/tsh/coercions/coercion.hpp>
#include <helios/tsh/coercions/coercion_error.hpp>
#include <helios/tsh/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>

#include <ranges>
#include <string>
#include <variant>

namespace compiler::helios::coercions {
	namespace {
		using tsh::coercions::CoercionError;

		namespace refusal_reason = tsh::coercions::coercion_error;

		/**
		 * @brief Get the main error cause from the @p refusal.
		 * @TODO: #3732 Do it better.
		 */
		const CoercionError& mainCause(const CoercionError& refusal) {
			CRef<CoercionError> cause = &refusal;
			// Go down the tree until we find the first non-SubPartRefused reason.
			while (v_matches(cause->reason, refusal_reason::SubPartRefused)) {
				CORE_ASSERT(
					not cause->causes.empty(),
					"A node with SubPartRefused should always have child in the error tree."
				);
				cause = &cause->causes.front().error;
			}
			return *cause;
		}

		/**
		 * @brief Nicely formatted list of all @p candidates for an ambiguous variant coercion @p
		 * refusal.
		 */
		std::string tiedAlternatives(
			const CoercionError& refusal, const std::vector<usize>& candidates
		) {
			CORE_ASSERT(
				refusal.target.getType().getKind() == tsh::Kind::Variant,
				"Tied alternatives if only valid for a variant type"
			);

			const tsh::VariantAbstractType variant{ refusal.target.getType() };
			return base::strJoin(
				candidates | std::views::transform([&](const usize candidate) {
					return base::strConcat(
						"`", variant.getUnderlyingTypes().at(candidate).toString(), "`"
					);
				}),
				", "
			);
		}
	}

	IncompatibleTypesError::IncompatibleTypesError(
		dia::StablePosition                 given_position,
		Box<InteractiveType>                actual_type,
		Box<InteractiveType>                expected_type,
		base::Optional<dia::StablePosition> coercion_expects_pos
	):
		  MessageWithCodeFragment(given_position) {
		addArgument<dia::InteractiveArgument>("given_type", std::move(actual_type));
		addArgument<dia::InteractiveArgument>("expected_type", std::move(expected_type));
		addPointerMessage("given", given_position);
		if_opt_some(coercion_expects_pos, expected_pos) {
			addPointerMessage("expected", expected_pos);
		}
	}

	NoMatchingExpectedTypeError::NoMatchingExpectedTypeError(
		dia::StablePosition given_position, Box<InteractiveType> actual_type
	):
		  MessageWithCodeFragment(given_position) {
		addArgument<dia::InteractiveArgument>("given_type", std::move(actual_type));
		addPointerMessage("given", given_position);
	}

	void NoMatchingExpectedTypeError::addExploreAcceptedType(
		std::string accepted_type, Box<dia::MessageBase> coercion_error
	) {
		auto id = dia::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(coercion_error));

		std::vector<Box<dia::Argument>> args;
		args.emplace_back(makeBox<dia::TextArgument>("accepted_type", std::move(accepted_type)));
		args.emplace_back(makeBox<dia::TextArgument>("message_id", id));
		this->addExploreLink("accepted_type", std::move(args));
	}

	const base::Optional<CoercionErrorOverrides::MessageReturner>& CoercionErrorOverrides::overrideFor(
		const tsh::coercions::CoercionFailure& reason
	) const {
		using namespace refusal_reason;
		using Override = const base::Optional<MessageReturner>&;

		return VARIANT_VISIT(
			reason,
			[&](const IncompatibleTypes&) -> Override { return incompatible_types; },
			[&](const MutabilityMismatch&) -> Override { return mutability_mismatch; },
			[&](const NotImplemented&) -> Override { return not_implemented; },
			[&](const AmbiguousCoercion&) -> Override { return ambiguous_coercion; },
			[&](const TypeNotCopyable&) -> Override { return type_not_copyable; },
			[&](const RequiresExplicitCopyMove&) -> Override { return requires_explicit_copy_move; },
			[&](const SubPartRefused&) -> Override {
				CORE_PANIC("SubPartRefused should not be reported directly");
			}
		)
	}

	Box<dia::MessageBase> getCoercionError(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		const CoercionErrorOverrides&       error_overrides
	) {
		const CoercionError& cause = mainCause(failed.getError());

		// Use the caller's override if one is set, otherwise the default message.
		const base::Optional<CoercionErrorOverrides::MessageReturner>& override
			= error_overrides.overrideFor(cause.reason);
		if_opt_some(override, message) { return message(); }

		using namespace refusal_reason;
		using Message = Box<dia::MessageBase>;

		return VARIANT_VISIT(
			cause.reason,
			[&](const IncompatibleTypes&) -> Message {
				return makeBox<IncompatibleTypesError>(
					source_position,
					makeBox<InteractiveType>(ctx, failed.getSource().getSymbolType()),
					makeBox<InteractiveType>(ctx, failed.getTarget()),
					coercion_expects_pos
				);
			},
			[&](const MutabilityMismatch&) -> Message {
				return makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Cannot hand a value of type `",
						cause.source.toString(),
						"` over to `",
						cause.target.toString(),
						"`, it would allow modifying an immutable value."
					),
					source_position
				);
			},
			[&](const SubPartRefused&) -> Message {
				CORE_PANIC("SubPartRefused should never be errored directly");
			},
			[&](const NotImplemented&) -> Message {
				return makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat(
						"Coercing `",
						cause.source.toString(),
						"` to `",
						cause.target.toString(),
						"`."
					),
					source_position
				);
			},
			[&](const AmbiguousCoercion& ambiguous) -> Message {
				return makeBox<dia::PlaceholderError>(
					base::strConcat(
						"A value of type `",
						cause.source.toString(),
						"` fits equally well into many alternatives of `",
						cause.target.toString(),
						"`: ",
						tiedAlternatives(cause, ambiguous.candidates),
						"."
					),
					source_position
				);
			},
			[&](const TypeNotCopyable& not_copyable) -> Message {
				return makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Type `", not_copyable.copied.type.toString(), "` cannot be copied."
					),
					source_position
				);
			},
			[&](const RequiresExplicitCopyMove& explicit_copy) -> Message {
				// Reading a value out of a `ref`/`box` copies the pointee. `move` out of ref/box
			    // isn't possible.
				const std::string advice
					= explicit_copy.copied.is_pointee
			            ? base::strConcat(
							  "` out of `",
							  cause.source.toString(),
							  "`. Use `copy` to copy it out, `move` cannot move a value out of a "
							  "reference."
						  )
			            : "`. Use `copy` to copy it or `move` to move it.";

				return makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Cannot implicitly copy a value of non-trivially-copyable type `",
						explicit_copy.copied.type.toString(),
						advice
					),
					source_position
				);
			}
		)
	}

	void logCoercionFailure(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		const CoercionErrorOverrides&       error_overrides
	) {
		ctx.logInt(
			getCoercionError(ctx, failed, source_position, coercion_expects_pos, error_overrides)
		);
	}

	void logNoMatchingExpectedTypeFailure(
		query::Context&              ctx,
		const std::vector<Coercion>& failed_coercions,
		dia::StablePosition          source_position
	) {
		CORE_ASSERT(not failed_coercions.empty(), "Called with empty failed coercions.");

		auto error = makeBox<NoMatchingExpectedTypeError>(
			source_position,
			makeBox<InteractiveType>(ctx, failed_coercions.front().getSource().getSymbolType())
		);

		// Every accepted type was tried, so each of them gets an explore link pointing to the error
		// explaining why the coercion to it failed.
		for (const Coercion& coercion: failed_coercions)
			error->addExploreAcceptedType(
				coercion.getTarget().toString(), getCoercionError(ctx, coercion, source_position)
			);

		ctx.logInt(std::move(error));
	}
}
