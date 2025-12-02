#include "errors.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/usage.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function_decl.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/errors/interactive_errors.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/utils/with_context_do.hpp>

namespace compiler::helios::code {
	using namespace dia_int;

	using ::compiler::helios::errors::InteractiveType;

	class FunctionDeclaredHereNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "function_declared_here" };
		}

	public:
		FunctionDeclaredHereNote(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class InteractiveFunctionName final: public dia_int::InteractiveElement {
		SymID function_symbol;

		Box<dia_int::dia_args::Component> getValue(dia_int::MessageBase& msg) final;

	public:
		InteractiveFunctionName(SymID function_symbol): function_symbol(function_symbol) {}

		// get function definition and attach function defined here
	};

	Box<dia_args::Component> InteractiveFunctionName::getValue(MessageBase& msg) {
		std::string              message_id;
		std::vector<std::string> linked_messages;
		std::string              displayed_name = name(function_symbol).str();

		query::utils::withContextDo([&](query::Context& ctx) {
			auto maybe_function = getSymRef(function_symbol)->getPSTData()->pst_element.unlock(ctx);
			if (maybe_function->getElementKind() == pst::ElementKind::Fun) {
				auto function = maybe_function.dynamicCast<pst::Fun>().value();
				auto fun_decl = function->getParams().unlock(ctx);
				auto id       = MessageBase::getUniqueID();
				msg.addLinkedMessage(
					id, makeBox<FunctionDeclaredHereNote>(fun_decl->getSourcePosition())
				);
				linked_messages.push_back(std::move(id));
			}
		});

		msg.addEntity<TextBasedEntity>(linked_messages, displayed_name);

		auto content = makeBox<dia_args::TextComponent>(displayed_name);
		auto link
			= makeBox<dia_args::LinkComponent>(std::move(linked_messages), std::move(content));
		return link;
	}

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
			dia::SourcePosition                          source_position,
			InteractiveType                              expected_type,
			InteractiveType                              actual_type,
			base::Optional<Box<InteractiveFunctionName>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			auto expected_type_box = makeBox<InteractiveType>(std::move(expected_type));
			auto actual_type_box   = makeBox<InteractiveType>(std::move(actual_type));

			addArgument<dia_int::InteractiveArgument>("expected_type", std::move(expected_type_box));
			addArgument<dia_int::InteractiveArgument>("given_type", std::move(actual_type_box));
			if (function_name.has_value())
				addArgument<dia_int::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class NamedArgumentDiplicateError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_duplicate_named" };
		}

	public:
		NamedArgumentDiplicateError(
			dia::SourcePosition                          source_position,
			base::Optional<Box<InteractiveFunctionName>> function_name
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
			dia::SourcePosition         call_source_position,
			dia::SourcePosition         declaration_source_position,
			base::Optional<std::string> function_name
		):
			  MessageWithCodeFragmentAndCause(declaration_source_position) {
			addArgument<CodeArgument>("call_code", call_source_position);
			addArgument<CodeLocationArgument>("call_code_location", call_source_position);
			addPointerMessage({ "call_cause", call_source_position });

			if (function_name.has_value())
				addArgument<dia_int::TextArgument>(
					"function_name", std::move(function_name.value())
				);
		}
	};

	class PositionalAfterNamedArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_arg_pos_after_named" };
		}

	public:
		PositionalAfterNamedArgumentError(
			dia::SourcePosition                          source_position,
			base::Optional<Box<InteractiveFunctionName>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			if (function_name.has_value())
				addArgument<dia_int::InteractiveArgument>(
					"function_name", std::move(function_name.value())
				);
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
			dia::SourcePosition                          source_position,
			base::Optional<Box<InteractiveFunctionName>> function_name
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
			dia::SourcePosition                          source_position,
			std::string                                  argument_name,
			base::Optional<Box<InteractiveFunctionName>> function_name
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::InteractiveArgument>(
				"function_name", std::move(function_name.value())
			);
			addArgument<dia_int::TextArgument>("argument_name", std::move(argument_name));
		}
	};

	// all the other classes

	// all other notes

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
			CORE_PANIC("Class methods not supported yet");
		}
		default:
			CORE_PANIC("Expected function or method declaration");
		}
	}

	pst::Access<pst::LangElement> getNthCallArgument(
		query::Context& ctx, pst::Access<pst::expr::Call> call_expr, usize argument_index
	) {
		usize current_index = 0;
		for (auto&& arg: *call_expr->getArgs().unlock(ctx)) {
			if (current_index == argument_index) return arg.unlock(ctx);
			current_index++;
		}
		CORE_PANIC("Argument index out of bounds");
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
		query::Context&              ctx,
		SymID                        function_symbol,
		pst::Access<pst::expr::Call> call_expr,
		const MatchFailure&          failure_reason,
		bool                         is_for_candidate_function_msg
	) {
		auto decl = getSymRef(function_symbol)->getPSTData()->pst_element.unlock(ctx);

		base::Optional<Box<InteractiveFunctionName>> function_name{};
		if (is_for_candidate_function_msg)
			function_name.emplace(makeBox<InteractiveFunctionName>(function_symbol));

		variant_match(failure_reason) {
			variant_case(DuplicateNamedArgument, data) {
				auto source_pos
					= getNthCallArgument(ctx, call_expr, data.argument_index)->getSourcePosition();
				return makeBox<NamedArgumentDiplicateError>(source_pos, std::move(function_name));
			}
			variant_case(TooManyCallArguments, data) {
				auto source_pos
					= getNthCallArgument(ctx, call_expr, data.valid_arguments)->getSourcePosition();
				// auto last_arg_pos = getNthCallArgument(ctx, call_expr, data.total_arguments - 1)
				//                         ->getSourcePosition();
				// Here I would like to create a source position that spans from source_pos to
				// last_arg_pos
				return makeBox<TooManyCallArgumentsError>(source_pos, std::move(function_name));
			}
			variant_case(UnknownNamedArgument, data) {
				auto arg_pos
					= getNthCallArgument(ctx, call_expr, data.argument_index)->getSourcePosition();
				return makeBox<UnknownNamedArgumentError>(
					arg_pos, data.name.str(), std::move(function_name)
				);
			}
			variant_case(TypeMismatch, data) {
				auto arg_expr = getNthCallArgument(ctx, call_expr, data.argument_index);

				return makeBox<ArgumentIncompatibleTypeError>(
					arg_expr->getSourcePosition(),
					InteractiveType(data.expected_type),
					InteractiveType(data.given_type),
					std::move(function_name)
				);
			}
			variant_case(MissingCallArgument, data) {
				auto param_decl = getNthDeclarationParameter(ctx, decl, data.parameter_index);

				base::Optional<std::string> function_name_str{};
				if (is_for_candidate_function_msg)
					function_name_str.emplace(name(function_symbol).str());

				return makeBox<CallMissingArgumentError>(
					call_expr->getSourcePosition(),
					param_decl->getSourcePosition(),
					std::move(function_name_str)
				);
			}
			variant_case(PositionalAfterNamedArgument, data) {
				auto arg_expr = getNthCallArgument(ctx, call_expr, data.argument_index);
				return makeBox<PositionalAfterNamedArgumentError>(
					arg_expr->getSourcePosition(), std::move(function_name)
				);
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
