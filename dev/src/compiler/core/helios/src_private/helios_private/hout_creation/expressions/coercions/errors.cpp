#include "errors.hpp"

#include "coercions.hpp"
#include "passing.hpp"

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>

namespace compiler::helios {
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

	Box<dia::MessageBase> getCoercionError(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos
	) {
		switch (failed.getInvalidReason()) {
		case InvalidCoercionReason::IncompatibleTypes:
			return makeBox<IncompatibleTypesError>(
				source_position,
				makeBox<InteractiveType>(ctx, failed.validated_from),
				makeBox<InteractiveType>(ctx, failed.to),
				coercion_expects_pos
			);
		case InvalidCoercionReason::TypeNotCopyable:
			return makeBox<dia::PlaceholderError>(
				base::strConcat(
					"Type `",
					copiedValueType(failed.validated_from, failed.to).toString(),
					"` cannot be copied."
				),
				source_position
			);
		case InvalidCoercionReason::RequiresExplicitCopyMove: {
			// Reading a value out of a `ref`/`box` copies the pointee. `move` out of ref/box isn't
			// possible, so suggesting it here is misleading.
			const bool reads_through
				= readsThroughReference(failed.validated_from.getRefKind(), failed.to.getRefKind());

			const std::string message
				= reads_through
			        ? base::strConcat(
						  "` out of `",
						  failed.validated_from.toString(),
						  "`. Use `copy` to copy it out, `move` cannot move a value out of a "
						  "reference."
					  )
			        : std::string("`. Use `copy` to copy it or `move` to move it.");

			return makeBox<dia::PlaceholderError>(
				base::strConcat(
					"Cannot implicitly copy a value of non-trivially-copyable type `",
					copiedValueType(failed.validated_from, failed.to).toString(),
					message
				),
				source_position
			);
		}

		default:
			CORE_UNREACHABLE();
		}
	}

	void logCoercionFailure(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides              error_overrides
	) {
		// Use the caller's override if one is set, otherwise the default message.
		const base::Optional<CoercionErrorOverrides::Logger>& override = [&]() -> const auto& {
			switch (failed.getInvalidReason()) {
			case InvalidCoercionReason::IncompatibleTypes:
				return error_overrides.incompatible_types;
			case InvalidCoercionReason::TypeNotCopyable:
				return error_overrides.type_not_copyable;
			case InvalidCoercionReason::RequiresExplicitCopyMove:
				return error_overrides.requires_explicit_copy_move;
			default:
				CORE_UNREACHABLE();
			}
		}();

		if (override.has_value()) {
			(*override)(ctx);
			return;
		}
		ctx.logInt(getCoercionError(ctx, failed, source_position, coercion_expects_pos));
	}

	void logNoMatchingExpectedTypeFailure(
		query::Context&              ctx,
		const std::vector<Coercion>& failed_coercions,
		dia::StablePosition          source_position
	) {
		CORE_ASSERT(not failed_coercions.empty(), "Called with empty failed coercions.");

		auto error = makeBox<NoMatchingExpectedTypeError>(
			source_position, makeBox<InteractiveType>(ctx, failed_coercions.at(0).validated_from)
		);

		// Every accepted type was tried, so each of them gets an explore link pointing to the error
		// explaining why the coercion to it failed.
		for (auto& coercion: failed_coercions)
			error->addExploreAcceptedType(
				coercion.to.toString(), getCoercionError(ctx, coercion, source_position)
			);

		ctx.logInt(std::move(error));
	}
}
