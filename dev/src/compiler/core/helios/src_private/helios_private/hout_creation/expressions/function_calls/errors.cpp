#include "errors.hpp"

#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/expressions/coercions/errors.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/placeholder.hpp>
#include <diagnostic/source_position.hpp>

namespace compiler::helios::code {
	using namespace dia;

	class PositionalAfterNamedArgumentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_pos_after_named" };
		}

	public:
		PositionalAfterNamedArgumentError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class RepeatedNamedArgumentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_named_repeated" };
		}

	public:
		RepeatedNamedArgumentError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ArgumentIncompatibleTypeError final: public dia::MessageWithCodeFragmentAndCause {
		// show given type and expected type
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_incompatible_type" };
		}

	public:
		ArgumentIncompatibleTypeError(
			dia::StablePosition                      source_position,
			Box<InteractiveType>                     expected_type,
			Box<InteractiveType>                     actual_type,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("expected_type", std::move(expected_type));
			addArgument<dia::InteractiveArgument>("given_type", std::move(actual_type));
			if (function_name.has_value())
				addArgument<dia::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class NamedArgumentProvidedByPositionalError final:
		  public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_named_repeats_pos" };
		}

	public:
		NamedArgumentProvidedByPositionalError(
			dia::StablePosition                      source_position,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			if (function_name.has_value())
				addArgument<dia::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class CallMissingArgumentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_missing_argument" };
		}

	public:
		CallMissingArgumentError(
			dia::StablePosition                 call_position,
			base::Optional<dia::StablePosition> missing_arg_position_opt
		):
			  MessageWithCodeFragmentAndCause(call_position) {
			if_opt_some(missing_arg_position_opt, missing_arg_position) {
				addArgument<CodeArgument>("missing_arg_code", missing_arg_position);
				addArgument<CodeLocationArgument>("missing_arg_code_location", missing_arg_position);
				addPointerMessage({ "missing_arg", missing_arg_position });
			}
		}
	};

	class TooManyCallArgumentsError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_too_many_args" };
		}

	public:
		TooManyCallArgumentsError(
			dia::StablePosition                      source_position,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			if (function_name.has_value())
				addArgument<dia::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class UnknownNamedArgumentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_unknown_named" };
		}

	public:
		UnknownNamedArgumentError(
			dia::StablePosition                      source_position,
			std::string                              argument_name,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("function_name", std::move(function_name.value()));
			addArgument<dia::TextArgument>("argument_name", std::move(argument_name));
		}
	};

	Box<dia::MessageBase> createDetailedCallErrorMessage(
		query::Context&            ctx,
		ElementOrigin              whole_call_origin,
		std::vector<ElementOrigin> arguments_origin,
		const CallFailure&         failure_reason,
		bool                       is_for_candidate_function
	) {
		variant_match(failure_reason) {
			variant_case(PositionalAfterNamedArgument, data) {
				auto source_pos = arguments_origin[data.argument_index].getStablePosition().value();
				return makeBox<PositionalAfterNamedArgumentError>(source_pos);
			}
			variant_case(RepeatedNamedArgument, data) {
				auto source_pos = arguments_origin[data.argument_index].getStablePosition().value();
				return makeBox<RepeatedNamedArgumentError>(source_pos);
			}
			variant_case(FunctionMatchFailure, data) {
				auto get_interactive_function
					= [&](SymID function_symbol) -> base::Optional<Box<InteractiveFunction>> {
					if (is_for_candidate_function)
						return std::nullopt;
					else
						return makeBox<InteractiveFunction>(ctx, function_symbol);
				};
				variant_match(data) {
					variant_case(TooManyCallArguments, data) {
						auto first_arg_pos
							= arguments_origin[data.valid_arguments].getStablePosition().value();
						auto last_arg_pos
							= arguments_origin[data.total_arguments - 1].getStablePosition().value();

						base::Optional<Box<InteractiveFunction>> function
							= get_interactive_function(data.function);
						auto pos = first_arg_pos.extendedWithSubsequentPos(last_arg_pos);
						return makeBox<TooManyCallArgumentsError>(pos, std::move(function));
					}
					variant_case(UnknownNamedArgument, data) {
						auto arg_pos
							= arguments_origin[data.argument_index].getStablePosition().value();
						base::Optional<Box<InteractiveFunction>> function_name
							= get_interactive_function(data.function);
						return makeBox<UnknownNamedArgumentError>(
							arg_pos, data.name.str(), std::move(function_name)
						);
					}
					variant_case(ArgumentCoercionFailure, data) {
						dia::StablePosition pos = [&] {
							if_opt_some(
								arguments_origin[data.argument_index].getStablePosition(), pos
							) {
								return pos;
							}
							return whole_call_origin.getStablePosition().value();
						}();

						if (data.failed.getInvalidReason()
						    == helios::InvalidCoercionReason::IncompatibleTypes) {
							base::Optional<Box<InteractiveFunction>> function_name
								= get_interactive_function(data.function);
							return makeBox<ArgumentIncompatibleTypeError>(
								pos,
								makeBox<InteractiveType>(ctx, data.failed.to),
								makeBox<InteractiveType>(ctx, data.failed.validated_from),
								std::move(function_name)
							);
						}

						return helios::getCoercionError(ctx, data.failed, pos);
					}
					variant_case(MissingCallArgument, data) {
						auto& decl = ctx.query<QueryDeclOfFun>(data.function)->valueOrThrow();

						if_opt_some(
							decl.parameters[data.parameter_index].origin.getStablePosition(),
							param_pos
						) {
							return makeBox<CallMissingArgumentError>(
								whole_call_origin.getStablePosition().value(), param_pos
							);
						}

						return makeBox<CallMissingArgumentError>(
							whole_call_origin.getStablePosition().value(), std::nullopt
						);
					}
					variant_case(NamedArgumentProvidedByPositional, data) {
						auto arg_pos
							= arguments_origin[data.argument_index].getStablePosition().value();
						base::Optional<Box<InteractiveFunction>> function_name
							= get_interactive_function(data.function);
						return makeBox<NamedArgumentProvidedByPositionalError>(
							arg_pos, std::move(function_name)
						);
					}
				}
			}
		}
		CORE_UNREACHABLE();
	}

	void AmbiguousMatchesError::addExploreExactCandidates(
		usize no_candidates, Box<dia::MessageBase> candidate_list
	) {
		auto id = dia::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia::Argument>> args;
		args.emplace_back(makeBox<dia::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia::TextArgument>("message_id", id));
		this->addExploreLink("exact_candidates", std::move(args));
	}

	void AmbiguousMatchesError::addExploreCoercibleCandidates(
		usize no_candidates, Box<dia::MessageBase> candidate_list
	) {
		auto id = dia::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia::Argument>> args;
		args.emplace_back(makeBox<dia::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia::TextArgument>("message_id", id));
		this->addExploreLink("coercible_candidates", std::move(args));
	}

	void AmbiguousMatchesError::addExploreFailedCandidates(
		usize no_candidates, Box<dia::MessageBase> candidate_list
	) {
		auto id = dia::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia::Argument>> args;
		args.emplace_back(makeBox<dia::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia::TextArgument>("message_id", id));
		this->addExploreLink("failed_candidates", std::move(args));
	}
}
