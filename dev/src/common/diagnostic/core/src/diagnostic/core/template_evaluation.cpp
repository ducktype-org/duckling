// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "template_evaluation.hpp"

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/diagnostic_state.hpp>
#include <diagnostic/core/exceptions.hpp>
#include <diagnostic/core/template_file.hpp>
#include <diagnostic/core/template_registry.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <diagnostic/core/yaml_buffer.hpp>
#include <filesystem/file.hpp>

#include <algorithm>

namespace dia {

	template<typename T>
	T getNewID() {
		static T current_id = 1;
		return current_id++;
	}

	// --------------------------------------------------------------------------------
	// Evaluation Contexts
	// --------------------------------------------------------------------------------

	/**
	 * @brief This is the evaluation context for the whole diagnostic.
	 * It contains the template registry and the diagnostic being evaluated.
	 * The diagnostic consists of multiple messages.
	 *
	 * It also keeps a mapping from diagnostic file message IDs to state message IDs.
	 */
	struct DiagnosticEvaluationContext final {
		TemplateRegistrySingleton&  registry;
		const dia_args::Diagnostic& diagnostic;

		base::HashMap<dia_args::MessageID, state::MessageID> message_mapping;
	};

	/**
	 * @brief This is the evaluation context for a single message.
	 * It keeps track of the pointer messages evaluated inside this message
	 * and the context of the evaluation.
	 */
	class MessageEvaluationContext final {
	private:
		// This is part of the result
		base::HashMap<dia_args::PointerMessage, state::PointerMessageID> pointer_message_mapping;
		base::HashMap<state::PointerMessageID, state::PointerMessage>    evaluated_pointer_messages;

	public:
		DiagnosticEvaluationContext& thread_ctx;

		// This is the main context for evaluating a message template

		/**
		 * @brief The arguments the evaluator sees during the evaluation of the tree.
		 */
		const base::HashMap<std::string, Box<dia_args::Component>>& arguments;

		/**
		 * @brief The parameters defined for the message template.
		 */
		const base::HashMap<std::string, template_file::Parameter>& parameters;

		/**
		 * @brief The macros the evaluator can use during the evaluation.
		 */
		const base::HashMap<std::string, Box<template_file::Component>>& macros;

		/**
		 * @brief The pointer messages provided by the message template.
		 */
		const base::HashMap<dia_args::PointerMessageID, template_file::PointerMessage>&
			pointer_messages;

		MessageEvaluationContext(
			DiagnosticEvaluationContext&                                     thread_ctx,
			const base::HashMap<std::string, Box<dia_args::Component>>&      arguments,
			const base::HashMap<std::string, template_file::Parameter>&      parameters,
			const base::HashMap<std::string, Box<template_file::Component>>& macros,
			const base::HashMap<dia_args::PointerMessageID, template_file::PointerMessage>&
				pointer_messages
		):
			  thread_ctx(thread_ctx),
			  arguments(arguments),
			  parameters(parameters),
			  macros(macros),
			  pointer_messages(pointer_messages) {}

		/**
		 * @brief Helper function that checks if a pointer message has already been evaluated
		 * and if not takes the correct pointer message (inside the current Message or outside of
		 * it) evaluates it and stores in the mapping.
		 *
		 * @param pm pointer message to evaluate
		 * @return state::PointerMessageID generated ID of the evaluated pointer message
		 */
		state::PointerMessageID evaluatePointerMessage(const dia_args::PointerMessage& pm);

		const base::HashMap<state::PointerMessageID, state::PointerMessage>& getEvaluatedPointerMessages(
		) const {
			return evaluated_pointer_messages;
		}
	};

	// --------------------------------------------------------------------------------
	// Visitors
	// --------------------------------------------------------------------------------


	/**
	 * @brief Evaluate template component tree into state component tree.
	 */
	class EvaluateTemplateFileVisitor final: public template_file::ComponentVisitor {
	public:
		MBox<state::Component>    res;
		MessageEvaluationContext& ctx;

		/**
		 * @brief The stack of message IDs collected during evaluation.
		 * The nested components can push/pop IDs to this stack,
		 * so in the state the leafs (text and code components) know which messages
		 * they link to and which pointer messages they
		 * highlight.
		 */
		std::vector<state::MessageID>&        current_message_ids;
		std::vector<state::PointerMessageID>& current_pointer_message_ids;

		EvaluateTemplateFileVisitor(
			MessageEvaluationContext&             ctx,
			std::vector<state::MessageID>&        current_message_ids,
			std::vector<state::PointerMessageID>& current_pointer_message_ids
		):
			  ctx(ctx),
			  current_message_ids(current_message_ids),
			  current_pointer_message_ids(current_pointer_message_ids) {}

		void visitTextComponent(const template_file::TextComponent& el) override;
		void visitConcatComponent(const template_file::ConcatComponent& el) override;
		void visitParamComponent(const template_file::ParamComponent& el) override;
		void visitIsParamProvidedComponent(const template_file::IsParamProvidedComponent& el
		) override;
		void visitMacroComponent(const template_file::MacroComponent& el) override;
		void visitCaseOfComponent(const template_file::CaseOfComponent& el) override;
		void visitCodeBlockComponent(const template_file::CodeBlockComponent& el) override;
		void visitMessageLinkComponent(const template_file::MessageLinkComponent& el) override;
		void visitVariantComponent(const template_file::VariantComponent& el) override;

		static Box<state::Component> evaluate(
			MessageEvaluationContext& ctx, CRef<template_file::Component> component
		);
	};

	/**
	 * @brief Convert diagnostic file components to state components.
	 */
	class EvaluateDiagnosticFileVisitor final: public dia_args::ComponentVisitor {
	public:
		MessageEvaluationContext& ctx;

		/**
		 * @brief The stack of message IDs collected during evaluation.
		 * The nested components can push/pop IDs to this stack,
		 * so in the state the leafs (text and code components) know which messages
		 * they link to and which pointer messages they
		 * highlight.
		 */
		std::vector<state::MessageID>&        current_message_ids;
		std::vector<state::PointerMessageID>& current_pointer_message_ids;

		MBox<state::Component> res;

		EvaluateDiagnosticFileVisitor(
			MessageEvaluationContext&             ctx,
			std::vector<state::MessageID>&        message_ids,
			std::vector<state::PointerMessageID>& pointer_message_ids
		):
			  ctx(ctx),
			  current_message_ids(message_ids),
			  current_pointer_message_ids(pointer_message_ids) {}

		void visitTextComponent(const dia_args::TextComponent& el) override;
		void visitCodeComponent(const dia_args::CodeComponent& el) override;
		void visitCodeLocationComponent(const dia_args::CodeLocationComponent& el) override;
		void visitStartLineComponent(const dia_args::StartLineComponent& el) override;
		void visitConcatComponent(const dia_args::ConcatComponent& el) override;
		void visitPointedComponent(const dia_args::PointedComponent& el) override;
		void visitVariantComponent(const dia_args::VariantComponent& el) override;
		void visitLinkComponent(const dia_args::LinkComponent& el) override;
		void visitEvaluatedTemplateComponent(const dia_args::EvaluatedTemplateComponent& el
		) override;
		void visitMessageIDComponent(const dia_args::MessageIDComponent& el) override;

		static Box<state::Component> evaluate(
			MessageEvaluationContext&             ctx,
			CRef<dia_args::Component>             component,
			std::vector<state::MessageID>&        message_ids,
			std::vector<state::PointerMessageID>& pointer_message_ids
		);
	};

	// --------------------------------------------------------------------------------
	// Helper Functions
	// --------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------
	// Visitor Implementations
	// --------------------------------------------------------------------------------


	// EvaluateTemplateFileVisitor

	Box<state::Component> EvaluateTemplateFileVisitor::evaluate(
		MessageEvaluationContext& ctx, CRef<template_file::Component> component
	) {
		std::vector<state::MessageID>        message_ids;
		std::vector<state::PointerMessageID> pointer_message_ids;
		EvaluateTemplateFileVisitor          header_visitor(ctx, message_ids, pointer_message_ids);
		component->acceptVisitor(header_visitor);
		if (header_visitor.res.toOpt().has_value())
			return std::move(header_visitor.res).toOptBox().value();
		throw TemplateEvaluationException("Template evaluation returned no component.");
	}

	void EvaluateTemplateFileVisitor::visitTextComponent(const template_file::TextComponent& el) {
		res = base::makeBox<state::TextComponent>(el.text, current_message_ids);
	}

	void EvaluateTemplateFileVisitor::visitConcatComponent(const template_file::ConcatComponent& el
	) {
		usize initial_message_ids = current_message_ids.size();
		usize initial_pointer_ids = current_pointer_message_ids.size();

		std::vector<Box<state::Component>> components;
		for (const auto& child: el.elements) {
			child->acceptVisitor(*this);
			if (auto result = std::move(res).toOptBox(); result.has_value())
				components.push_back(std::move(result).value());
			else
				throw TemplateEvaluationException(
					"Template component evaluation returned no component."
				);
		}

		if (current_message_ids.size() != initial_message_ids) {
			throw TemplateEvaluationException(
				"Message IDs stack corrupted during template evaluation."
			);
		}
		if (current_pointer_message_ids.size() != initial_pointer_ids) {
			throw TemplateEvaluationException(
				"Pointer message IDs stack corrupted during template evaluation."
			);
		}

		res = base::makeBox<state::ConcatComponent>(std::move(components));
	}

	void EvaluateTemplateFileVisitor::visitParamComponent(const template_file::ParamComponent& el) {
		if (not ctx.arguments.contains(el.param)) {
			throw TemplateEvaluationException(
				base::strConcat("Parameter '", el.param, "' not provided.")
			);
		}

		auto argument = ctx.arguments.at(el.param).ref();
		res           = EvaluateDiagnosticFileVisitor::evaluate(
            ctx, argument, current_message_ids, current_pointer_message_ids
        );

		if (not ctx.parameters.contains(el.param)) {
			throw TemplateEvaluationException(
				base::strConcat("Parameter '", el.param, "' is not declared by the template.")
			);
		}
		const auto& param = ctx.parameters.at(el.param);
		if (param.argument_expected_type.has_value()
		    && param.argument_expected_type.value() != argument->getTypeName()) {
			throw TemplateEvaluationException(base::strConcat(
				"Parameter '",
				el.param,
				"' has incorrect type. Expected: '",
				param.argument_expected_type.value(),
				"', got: '",
				argument->getTypeName(),
				"'."
			));
		}
	}

	void EvaluateTemplateFileVisitor::visitIsParamProvidedComponent(
		const template_file::IsParamProvidedComponent& el
	) {
		std::string text = ctx.arguments.contains(el.param) ? "true" : "false";
		res              = base::makeBox<state::TextComponent>(text, current_message_ids);
	}

	void EvaluateTemplateFileVisitor::visitMacroComponent(const template_file::MacroComponent& el) {
		if (auto macro = ctx.macros.atMaybe(el.macro); macro.has_value())
			(*macro.value())->acceptVisitor(*this);
		else
			throw TemplateEvaluationException(base::strConcat("Macro '", el.macro, "' not defined.")
			);
	}

	void EvaluateTemplateFileVisitor::visitCaseOfComponent(const template_file::CaseOfComponent& el
	) {
		el.pattern->acceptVisitor(*this);

		if (!this->res.toOpt().has_value())
			throw TemplateEvaluationException("Pattern evaluation failed.");

		std::string pattern_value = constructTextView(this->res.toOpt().value());

		if (auto case_content = el.cases.atMaybe(pattern_value); case_content.has_value())
			(*case_content.value())->acceptVisitor(*this);
		else if (auto default_case = el.cases.atMaybe("default"); default_case.has_value())
			(*default_case.value())->acceptVisitor(*this);
		else
			throw TemplateEvaluationException(
				base::strConcat("No case matched for pattern value: ", pattern_value)
			);
	}

	void EvaluateTemplateFileVisitor::visitCodeBlockComponent(
		const template_file::CodeBlockComponent& el
	) {
		el.code_elements->acceptVisitor(*this);
		auto content = std::move(res).toOptBox();

		if (!content.has_value())
			throw TemplateEvaluationException("CodeBlock evaluation returned no component.");

		base::Optional<CodeLocation> location;
		if (el.location.ref().toOpt().has_value()) {
			el.location.ref().toOpt().value()->acceptVisitor(*this);
			if (auto loc_res = std::move(res).toOptBox(); loc_res.has_value()) {
				if (auto loc_comp
				    = dynamic_cast<state::CodeLocationComponent*>(loc_res.value().get())) {
					location = loc_comp->location;
				}
			}
		}

		res = base::makeBox<state::CodeBlockComponent>(std::move(content).value(), location);
	}

	void EvaluateTemplateFileVisitor::visitMessageLinkComponent(
		const template_file::MessageLinkComponent& el
	) {
		el.target_message->acceptVisitor(*this);

		if (!this->res.toOpt().has_value())
			throw TemplateEvaluationException("Message link target evaluation failed.");

		std::string message_id_str = constructTextView(this->res.toOpt().value());

		if (not ctx.thread_ctx.message_mapping.contains(message_id_str))
			throw TemplateEvaluationException(base::strConcat(
				"Message link target ID '", message_id_str, "' not found in thread."
			));

		current_message_ids.push_back(ctx.thread_ctx.message_mapping.at(message_id_str));
		std::cout << "Visiting MessageLinkComponent, pushing message ID: "
				  << ctx.thread_ctx.message_mapping.at(message_id_str) << "\n";
		el.content->acceptVisitor(*this);
		current_message_ids.pop_back();
	}

	void EvaluateTemplateFileVisitor::visitVariantComponent(const template_file::VariantComponent& el
	) {
		el.content->acceptVisitor(*this);
		auto primary = std::move(res).toOptBox();
		if (!primary.has_value())
			throw TemplateEvaluationException("Variant primary content evaluation failed.");

		el.alt_content->acceptVisitor(*this);
		auto alternative = std::move(res).toOptBox();
		if (!alternative.has_value())
			throw TemplateEvaluationException("Variant alternative content evaluation failed.");

		res = base::makeBox<state::InteractiveComponent>(
			state::ComponentID(0), std::move(primary).value(), std::move(alternative).value()
		);
	}

	// EvaluateDiagnosticFileVisitor

	Box<state::Component> EvaluateDiagnosticFileVisitor::evaluate(
		MessageEvaluationContext&             ctx,
		CRef<dia_args::Component>             component,
		std::vector<state::MessageID>&        message_ids,
		std::vector<state::PointerMessageID>& pointer_message_ids
	) {
		EvaluateDiagnosticFileVisitor visitor(ctx, message_ids, pointer_message_ids);
		component->acceptVisitor(visitor);
		if (visitor.res.toOpt().has_value()) return std::move(visitor.res).toOptBox().value();
		throw TemplateEvaluationException("Diagnostic file component evaluation failed.");
	}

	void EvaluateDiagnosticFileVisitor::visitTextComponent(const dia_args::TextComponent& el) {
		res = base::makeBox<state::TextComponent>(el.content, current_message_ids);
	}

	void EvaluateDiagnosticFileVisitor::visitCodeComponent(const dia_args::CodeComponent& el) {
		res = base::makeBox<state::CodeComponent>(
			state::ComponentID(0), el.content, current_pointer_message_ids, current_message_ids
		);
	}

	void EvaluateDiagnosticFileVisitor::visitCodeLocationComponent(
		const dia_args::CodeLocationComponent& el
	) {
		res = base::makeBox<state::CodeLocationComponent>(el.location);
	}

	void EvaluateDiagnosticFileVisitor::visitStartLineComponent(const dia_args::StartLineComponent& el
	) {
		res = base::makeBox<state::StartLineComponent>(el.number);
	}

	void EvaluateDiagnosticFileVisitor::visitConcatComponent(const dia_args::ConcatComponent& el) {
		usize initial_message_ids = current_message_ids.size();
		usize initial_pointer_ids = current_pointer_message_ids.size();

		std::vector<Box<state::Component>> components;
		for (const auto& child: el.elements) {
			child->acceptVisitor(*this);
			if (auto result = std::move(res).toOptBox(); result.has_value())
				components.push_back(std::move(result).value());
		}

		if (current_message_ids.size() != initial_message_ids)
			throw TemplateEvaluationException("Message IDs stack corrupted.");
		if (current_pointer_message_ids.size() != initial_pointer_ids)
			throw TemplateEvaluationException("Pointer message IDs stack corrupted.");

		res = base::makeBox<state::ConcatComponent>(std::move(components));
	}

	void EvaluateDiagnosticFileVisitor::visitPointedComponent(const dia_args::PointedComponent& el) {
		for (const auto& pm_id: el.pointer_messages)
			current_pointer_message_ids.push_back(ctx.evaluatePointerMessage(pm_id));

		el.content->acceptVisitor(*this);

		for (size_t i = 0; i < el.pointer_messages.size(); ++i)
			current_pointer_message_ids.pop_back();
	}

	void EvaluateDiagnosticFileVisitor::visitVariantComponent(const dia_args::VariantComponent& el) {
		// We need to catch exceptions here? Or just let them propagate?
		// The original code panicked if either failed.
		auto primary = EvaluateDiagnosticFileVisitor::evaluate(
			ctx, el.content.ref(), current_message_ids, current_pointer_message_ids
		);

		auto alternative = EvaluateDiagnosticFileVisitor::evaluate(
			ctx, el.alt_content.ref(), current_message_ids, current_pointer_message_ids
		);

		res = base::makeBox<state::InteractiveComponent>(
			state::ComponentID(0), std::move(primary), std::move(alternative)
		);
	}

	void EvaluateDiagnosticFileVisitor::visitLinkComponent(const dia_args::LinkComponent& el) {
		auto no_links = el.target_messages.size();

		for (const auto& attached_message: el.target_messages)
			current_message_ids.push_back(ctx.thread_ctx.message_mapping.at(attached_message));

		el.content->acceptVisitor(*this);

		for (size_t i = 0; i < no_links; ++i) current_message_ids.pop_back();
	}

	// Forward declaration needed for visitEvaluatedTemplateComponent
	Box<state::Component> evaluateTemplateIfComponent(
		DiagnosticEvaluationContext&             thread_ctx,
		const template_file::DiagnosticTemplate& generic_template,
		const dia_args::Message&                 message
	);

	void EvaluateDiagnosticFileVisitor::visitEvaluatedTemplateComponent(
		const dia_args::EvaluatedTemplateComponent& el
	) {
		auto attached_msg = ctx.thread_ctx.diagnostic.linked_messages.atMaybe(el.message_id);
		if (!attached_msg.has_value()) {
			throw TemplateEvaluationException(base::strConcat(
				"Evaluated template message ID '", el.message_id, "' not found in diagnostic thread."
			));
		}

		const auto& msg = *attached_msg.value();

		auto& diagnostic_template = ctx.thread_ctx.registry.loadTemplate(msg.metadata);
		res = evaluateTemplateIfComponent(ctx.thread_ctx, diagnostic_template, msg);
	}

	void EvaluateDiagnosticFileVisitor::visitMessageIDComponent(const dia_args::MessageIDComponent& el
	) {
		res = base::makeBox<state::TextComponent>(el.message_id);
		if (!ctx.thread_ctx.diagnostic.linked_messages.contains(el.message_id)) {
			throw TemplateEvaluationException(
				base::strConcat("Message ID '", el.message_id, "' not found in diagnostic thread.")
			);
		}
	}

	// --------------------------------------------------------------------------------
	// Main evaluation Logic
	// --------------------------------------------------------------------------------

	Box<state::Component> evaluateTemplateIfComponent(
		DiagnosticEvaluationContext&             thread_ctx,
		const template_file::DiagnosticTemplate& generic_template,
		const dia_args::Message&                 message
	) {
		return std::visit(
			[&](auto&& tmpl) -> Box<state::Component> {
				using T = std::decay_t<decltype(tmpl)>;
				if constexpr (std::is_same_v<T, template_file::ComponentTemplate>) {
					MessageEvaluationContext ctx(
						thread_ctx, message.arguments, tmpl.params, tmpl.macros, {}
					);
					return EvaluateTemplateFileVisitor::evaluate(ctx, tmpl.content.ref());
				} else {
					throw TemplateEvaluationException("The template is not a component template.");
				}
			},
			generic_template.content
		);
	}

	state::Message evaluateMessage(
		DiagnosticEvaluationContext&          thread_ctx,
		const template_file::MessageTemplate& message_template,
		const dia_args::Message&              message
	) {
		MessageEvaluationContext ctx(
			thread_ctx,
			message.arguments,
			message_template.params,
			message_template.macros,
			message_template.pointer_messages
		);

		auto header
			= EvaluateTemplateFileVisitor::evaluate(ctx, message_template.header_message.ref());

		base::MBox<state::Component> description;
		if (message_template.description.ref().toOpt().has_value()) {
			auto desc = EvaluateTemplateFileVisitor::evaluate(
				ctx, message_template.description.ref().toOpt().value()
			);
			description = base::MBox<state::Component>(std::move(desc));
		}

		std::vector<state::ExploreEdge> evaluated_explore_links;
		for (const auto& edge_input: message.explore_links) {
			if (not message_template.explore_links.contains(edge_input.name)) {
				throw TemplateEvaluationException(
					base::strConcat("Explore edge '", edge_input.name, "' not defined in template.")
				);
			}

			const auto& edge_template = message_template.explore_links.at(edge_input.name);

			MessageEvaluationContext edge_ctx(
				thread_ctx, edge_input.params, edge_template.params, message_template.macros, {}
			);

			auto content
				= EvaluateTemplateFileVisitor::evaluate(edge_ctx, edge_template.content.ref());
			evaluated_explore_links.emplace_back(state::ExploreEdge{
				.name = edge_input.name, .content = std::move(content) });
		}

		state::Message result(
			message_template.metadata,
			std::move(header),
			std::move(description),
			ctx.getEvaluatedPointerMessages(),
			std::move(evaluated_explore_links)
		);
		return result;
	}

	/**
	 * @brief Evaluates a diagnostic message template into a state message if the template is a
	 * message template.
	 */
	state::Message evaluateTemplateIfMessage(
		DiagnosticEvaluationContext&             thread_ctx,
		const template_file::DiagnosticTemplate& generic_template,
		const dia_args::MessageID&               message_id,
		const dia_args::Message&                 message
	) {
		try {
			return std::visit(
				[&](auto&& tmpl) -> state::Message {
					using T = std::decay_t<decltype(tmpl)>;
					if constexpr (std::is_same_v<T, template_file::MessageTemplate>)
						return evaluateMessage(thread_ctx, tmpl, message);
					else
						throw TemplateEvaluationException("The template is not a message template.");
				},
				generic_template.content
			);
		} catch (const TemplateEvaluationException& e) {
			throw TemplateEvaluationException(
				base::strConcat("Error while evaluating message '", message_id, "': ", e.what())
			);
		}
	}

	/**
	 * @brief Evaluates a diagnostic pointer message template into a state pointer message if the
	 * template is a pointer message template.
	 */
	state::PointerMessage evaluateTemplateIfPointerMessage(
		DiagnosticEvaluationContext&             thread_ctx,
		const template_file::DiagnosticTemplate& generic_template,
		const dia_args::Message&                 pointer_message,
		const dia_args::PointerMessageID&        pm_id
	) {
		try {
			return std::visit(
				[&](auto&& tmpl) -> state::PointerMessage {
					using T = std::decay_t<decltype(tmpl)>;
					if constexpr (std::is_same_v<T, template_file::PointerMessageTemplate>) {
						MessageEvaluationContext ctx(
							thread_ctx,
							pointer_message.arguments,
							tmpl.params,
							tmpl.macros,
							tmpl.pointer_messages
						);
						if (!tmpl.pointer_messages.contains(pm_id)) {
							throw TemplateEvaluationException(base::strConcat(
								"Pointer message ID '", pm_id, "' not found in template."
							));
						}

						auto& dia_args_pointer_message = tmpl.pointer_messages.at(pm_id);
						auto  content_component        = EvaluateTemplateFileVisitor::evaluate(
                            ctx, dia_args_pointer_message.content.ref()
                        );
						auto text_view = constructTextView(content_component.ref());
						return state::PointerMessage{
							.type     = dia_args_pointer_message.type,
							.content  = std::move(text_view),
							.priority = static_cast<u32>(dia_args_pointer_message.priority)
						};
					} else {
						throw TemplateEvaluationException(
							"The template is not a pointer message template."
						);
					}
				},
				generic_template.content
			);
		} catch (const TemplateEvaluationException& e) {
			throw TemplateEvaluationException(
				base::strConcat("Error while evaluating pointer message '", pm_id, "': ", e.what())
			);
		}
	}

	state::PointerMessageID MessageEvaluationContext::evaluatePointerMessage(
		const dia_args::PointerMessage& pm
	) {
		if (this->pointer_message_mapping.contains(pm)) return this->pointer_message_mapping.at(pm);

		if (pm.message_id.empty()) {
			auto& pointer_message_content = pointer_messages.at(pm.pointer_message_id);
			auto  evaluated_component     = EvaluateTemplateFileVisitor::evaluate(
                *this, pointer_message_content.content.ref()
            );

			auto text_view = constructTextView(evaluated_component.ref());
			auto new_id    = getNewID<state::PointerMessageID>();
			this->evaluated_pointer_messages.put(
				new_id,
				state::PointerMessage{ .type    = pointer_message_content.type,
			                           .content = std::move(text_view),
			                           .priority
			                           = static_cast<u32>(pointer_message_content.priority) }
			);
			this->pointer_message_mapping.insert_or_assign(pm, new_id);
			return new_id;
		} else {
			const auto& message = thread_ctx.diagnostic.linked_messages.at(pm.message_id.value());
			auto&       diagnostic_template = thread_ctx.registry.loadTemplate(message.metadata);
			auto        evaluated_pointer_message = evaluateTemplateIfPointerMessage(
                thread_ctx, diagnostic_template, message, pm.pointer_message_id
            );
			auto new_id = getNewID<state::PointerMessageID>();
			this->evaluated_pointer_messages.put(new_id, std::move(evaluated_pointer_message));
			this->pointer_message_mapping.insert_or_assign(pm, new_id);
			return new_id;
		}
	}

	/**
	 * @brief Get the order of messages to be displayed, starting from the main message.
	 * This is left to right tree traversal of attaches messages and linkes inside messages.
	 * First the attached messages are traversed, then the linked messages inside the message.
	 */
	void getMessageOrder(
		const DiagnosticEvaluationContext& ctx,
		const std::vector<state::Message>& evaluated_messages,
		std::vector<state::MessageID>&     output,
		const dia_args::Message&           msg,
		const state::Message&              evaluated_msg
	) {
		for (auto& msg_id: msg.attached_messages) {
			auto evaluated_id = ctx.message_mapping.at(msg_id);
			if (std::ranges::find(output, evaluated_id) != output.end()) continue;

			output.push_back(evaluated_id);
			getMessageOrder(
				ctx,
				evaluated_messages,
				output,
				ctx.diagnostic.linked_messages.at(msg_id),
				evaluated_messages[evaluated_id]
			);
		}

		for (auto& msg_id: evaluated_msg.getOrderedMessageLinks()) {
			if (std::ranges::find(output, msg_id) != output.end()) continue;

			output.push_back(msg_id);
			getMessageOrder(
				ctx,
				evaluated_messages,
				output,
				ctx.diagnostic.linked_messages.at(
					std::ranges::find_if(
						ctx.message_mapping, [&](const auto& pair) { return pair.second == msg_id; }
					)->first
				),
				evaluated_messages[msg_id]
			);
		}
	}

	state::Diagnostic evaluateDiagnostic(const dia_args::Diagnostic& thread) {
		const state::MessageID     main_message_id = 0;
		TemplateRegistrySingleton& registry        = TemplateRegistrySingleton::getInstance();

		DiagnosticEvaluationContext thread_ctx{
			.registry        = registry,
			.diagnostic      = thread,
			.message_mapping = { { "<<main_message_not_used_id>>", main_message_id } },
		};
		// Step 1. assigning IDs to additional messages, so the mapping is ready
		state::MessageID message_id_generator = 1;
		for (const auto& [message_id, message]: thread.linked_messages)
			if (message.metadata.template_type == "message")
				thread_ctx.message_mapping.insert_or_assign(message_id, message_id_generator++);

		std::vector<state::Message> messages;

		// Step 2. Evaluate main message and additional messages
		auto& main_template = registry.loadTemplate(thread.main_message.metadata);
		messages.push_back(evaluateTemplateIfMessage(
			thread_ctx, main_template, "main_message", thread.main_message
		));

		for (const auto& [message_id, message]: thread.linked_messages) {
			if (message.metadata.template_type == "message") {
				messages.push_back(evaluateTemplateIfMessage(
					thread_ctx, registry.loadTemplate(message.metadata), message_id, message
				));
			}
		}

		// Load the main message template
		std::vector<state::MessageID> displayed_messages = { 0 };
		getMessageOrder(thread_ctx, messages, displayed_messages, thread.main_message, messages[0]);

		return { std::move(displayed_messages), std::move(messages) };
	}

}
