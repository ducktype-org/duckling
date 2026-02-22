#include "errors.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/message.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function_decl.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::helios::code {
	using namespace dia_int;

	class PositionalAfterNamedArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_pos_after_named" };
		}

	public:
		PositionalAfterNamedArgumentError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class RepeatedNamedArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_named_repeated" };
		}

	public:
		RepeatedNamedArgumentError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ArgumentIncompatibleTypeError final: public dia_int::MessageWithCodeFragmentAndCause {
		// show given type and expected type
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_incompatible_type" };
		}

	public:
		ArgumentIncompatibleTypeError(
			dia::SourcePosition                      source_position,
			Box<InteractiveType>                     expected_type,
			Box<InteractiveType>                     actual_type,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::InteractiveArgument>("expected_type", std::move(expected_type));
			addArgument<dia_int::InteractiveArgument>("given_type", std::move(actual_type));
			if (function_name.has_value())
				addArgument<dia_int::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class NamedArgumentProvidedByPositionalError final:
		  public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_named_repeats_pos" };
		}

	public:
		NamedArgumentProvidedByPositionalError(
			dia::SourcePosition                      source_position,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			if (function_name.has_value())
				addArgument<dia_int::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class CallMissingArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_missing_argument" };
		}

	public:
		CallMissingArgumentError(
			dia::SourcePosition                 call_position,
			base::Optional<dia::SourcePosition> missing_arg_position_opt
		):
			  MessageWithCodeFragmentAndCause(call_position) {
			if_opt_some(missing_arg_position_opt, missing_arg_position) {
				addArgument<CodeArgument>("missing_arg_code", missing_arg_position);
				addArgument<CodeLocationArgument>("missing_arg_code_location", missing_arg_position);
				addPointerMessage({ "missing_arg", missing_arg_position });
			}
		}
	};

	class TooManyCallArgumentsError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_too_many_args" };
		}

	public:
		TooManyCallArgumentsError(
			dia::SourcePosition                      source_position,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			if (function_name.has_value())
				addArgument<dia_int::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class UnknownNamedArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_unknown_named" };
		}

	public:
		UnknownNamedArgumentError(
			dia::SourcePosition                      source_position,
			std::string                              argument_name,
			base::Optional<Box<InteractiveFunction>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::InteractiveArgument>(
				"function_name", std::move(function_name.value())
			);
			addArgument<dia_int::TextArgument>("argument_name", std::move(argument_name));
		}
	};

	/**
	 * Documentation in the header file.
	 */
	pst::Access<pst::ParamList> getFunctionParamList(
		query::Context& ctx, pst::Access<pst::LangElement> function_decl
	) {
		switch (function_decl->getElementKind()) {
		case pst::ElementKind::Fun: {
			auto fun = function_decl.dynamicCast<pst::Fun>().value();
			return fun->getParams().unlock(ctx);
		}
		case pst::ElementKind::FunDecl: {
			auto fun_decl = function_decl.dynamicCast<pst::FunDecl>().value();
			return fun_decl->getParams().unlock(ctx);
		}
		case pst::ElementKind::ClassMethod: {
			// @TODO: #1547 When class methods are being implemented think of the errors messages
			auto class_method = function_decl.dynamicCast<pst::Method>().value();
			return class_method->getParams().unlock(ctx);
		}
		default:
			CORE_PANIC("Expected function or method declaration");
		}
	}

	pst::Access<pst::LangElement> getNthDeclarationParameter(
		query::Context& ctx, pst::Access<pst::LangElement> function_decl, usize parameter_index
	) {
		usize current_index = 0;
		for (auto&& param: *getFunctionParamList(ctx, function_decl)) {
			if (current_index == parameter_index) return param.unlock(ctx);
			current_index++;
		}
		CORE_PANIC("Parameter index out of bounds");
	}

	Box<dia_int::MessageBase> createDetailedCallErrorMessage(
		query::Context&     ctx,
		CallSourcePositions source_positions,
		const CallFailure&  failure_reason,
		bool                is_for_candidate_function
	) {
		variant_match(failure_reason) {
			variant_case(PositionalAfterNamedArgument, data) {
				return makeBox<PositionalAfterNamedArgumentError>(
					source_positions.arguments.at(data.argument_index)
				);
			}
			variant_case(RepeatedNamedArgument, data) {
				return makeBox<RepeatedNamedArgumentError>(
					source_positions.arguments.at(data.argument_index)
				);
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
						auto first_arg_pos = source_positions.arguments.at(data.valid_arguments);
						auto last_arg_pos = source_positions.arguments.at(data.total_arguments - 1);
						base::Optional<Box<InteractiveFunction>> function
							= get_interactive_function(data.function);
						auto pos = dia::SourcePosition::merge(first_arg_pos, last_arg_pos);
						return makeBox<TooManyCallArgumentsError>(pos, std::move(function));
					}
					variant_case(UnknownNamedArgument, data) {
						auto arg_pos = source_positions.arguments.at(data.argument_index);
						base::Optional<Box<InteractiveFunction>> function_name
							= get_interactive_function(data.function);
						return makeBox<UnknownNamedArgumentError>(
							arg_pos, data.name.str(), std::move(function_name)
						);
					}
					variant_case(TypeMismatch, data) {
						auto arg_pos = source_positions.arguments.at(data.argument_index);
						base::Optional<Box<InteractiveFunction>> function_name
							= get_interactive_function(data.function);
						return makeBox<ArgumentIncompatibleTypeError>(
							arg_pos,
							makeBox<InteractiveType>(ctx, data.expected_type),
							makeBox<InteractiveType>(ctx, data.given_type),
							std::move(function_name)
						);
					}
					variant_case(MissingCallArgument, data) {
						if_opt_some(
							getSymRef(data.function)->getDataOpt<PstSymbolData>(), pst_data
						) {
							auto param_decl = getNthDeclarationParameter(
								ctx, pst_data->pst_element.unlock(ctx), data.parameter_index
							);
							base::Optional<dia::SourcePosition> param_position{
								param_decl->getSourcePosition()
							};
							return makeBox<CallMissingArgumentError>(
								source_positions.whole, param_position
							);
						}

						return makeBox<CallMissingArgumentError>(
							source_positions.whole, std::nullopt
						);
					}
					variant_case(NamedArgumentProvidedByPositional, data) {
						auto arg_pos = source_positions.arguments.at(data.argument_index);
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
		usize no_candidates, Box<dia_int::MessageBase> candidate_list
	) {
		auto id = dia_int::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia_int::Argument>> args;
		args.emplace_back(
			makeBox<dia_int::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia_int::TextArgument>("message_id", id));
		this->addExploreLink("exact_candidates", std::move(args));
	}

	void AmbiguousMatchesError::addExploreCoercibleCandidates(
		usize no_candidates, Box<dia_int::MessageBase> candidate_list
	) {
		auto id = dia_int::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia_int::Argument>> args;
		args.emplace_back(
			makeBox<dia_int::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia_int::TextArgument>("message_id", id));
		this->addExploreLink("coercible_candidates", std::move(args));
	}

	void AmbiguousMatchesError::addExploreFailedCandidates(
		usize no_candidates, Box<dia_int::MessageBase> candidate_list
	) {
		auto id = dia_int::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(candidate_list));
		std::vector<Box<dia_int::Argument>> args;
		args.emplace_back(
			makeBox<dia_int::TextArgument>("no_candidates", std::to_string(no_candidates))
		);
		args.emplace_back(makeBox<dia_int::TextArgument>("message_id", id));
		this->addExploreLink("failed_candidates", std::move(args));
	}
}
