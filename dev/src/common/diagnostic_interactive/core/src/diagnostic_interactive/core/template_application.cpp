#include "template_application.hpp"

#include "base/str_utils.hpp"

namespace dia_app {

	// EvaluateToTextVisitor methods

	void EvaluateToTextVisitor::visitTextComponent(const state::TextComponent& el) {
		result += el.content;
	}

	void EvaluateToTextVisitor::visitCodeComponent(const state::CodeComponent& el) {
		result += el.content;
	}

	void EvaluateToTextVisitor::visitConcatComponent(const state::ConcatComponent& el) {
		for (const auto& component: el.components) (*component).acceptVisitor(*this);
	}

	void EvaluateToTextVisitor::visitInteractiveComponent(const state::InteractiveComponent& el) {
		(*el.primary).acceptVisitor(*this);
	}

	void EvaluateToTextVisitor::visitStartLineComponent(const state::StartLineComponent& el) {
		// StartLineComponent doesn't produce visible text
	}

	void EvaluateToTextVisitor::visitCodeBlockComponent(const state::CodeBlockComponent& el) {
		(*el.content).acceptVisitor(*this);
	}

	// TemplateRegistry methods
	template_file::DiagnosticTemplate& TemplateRegistry::loadTemplate(
		const dia_file::Metadata& metadata
	) {
		std::string key = metadata.type + "/" + metadata.family + "/" + metadata.name;

		if (auto cached = cache.atMaybe(key); cached.has_value()) return *cached.value();

		// Construct file path
		std::string path = templates_root + "/" + metadata.type + "/" + metadata.family + "/"
		                 + metadata.name + ".yaml";

		// Load and parse YAML file
		YAML::Node yaml_node           = YAML::LoadFile(path);
		auto       diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml_node);

		cache.put(key, std::move(diagnostic_template));
		return cache[key];
	}

	// EvaluateTemplateVisitor methods
	void EvaluateTemplateVisitor::visitTextComponent(const template_file::TextComponent& el) {
		res = base::makeBox<state::TextComponent>(el.text, current_message_ids);
	}

	void EvaluateTemplateVisitor::visitConcatComponent(const template_file::ConcatComponent& el) {
		usize initial_message_ids = current_message_ids.size();
		usize initial_pointer_ids = current_pointer_message_ids.size();

		std::vector<Box<state::Component>> components;
		for (const auto& child: el.elements) {
			child->acceptVisitor(*this);
			if (auto result = std::move(res).toOptBox(); result.has_value())
				components.push_back(std::move(result).value());
			else
				CORE_PANIC("Template component evaluation returned no component.");
		}

		// Verify stacks weren't corrupted
		CORE_ASSERT(
			current_message_ids.size() == initial_message_ids,
			"Message IDs stack corrupted during template evaluation."
		);
		CORE_ASSERT(
			current_pointer_message_ids.size() == initial_pointer_ids,
			"Pointer message IDs stack corrupted during template evaluation."
		);

		res = base::makeBox<state::ConcatComponent>(std::move(components));
	}

	void EvaluateTemplateVisitor::visitParamComponent(const template_file::ParamComponent& el) {
		// Look up parameter from diagnostic file and convert it
		if (auto param = ctx.params.atMaybe(el.param); param.has_value()) {
			// Convert diagnostic file component to state component
			EvaluateDiagnosticFileVisitor converter(
				ctx, current_message_ids, current_pointer_message_ids
			);
			(*param.value())->acceptVisitor(converter);
			res = std::move(converter.res);
		} else {
			CORE_PANIC("Parameter '%s' not provided.", el.param.c_str());
		}
	}

	void EvaluateTemplateVisitor::visitIsParamProvidedComponent(
		const template_file::IsParamProvidedComponent& el
	) {
		std::string text = ctx.params.contains(el.param) ? "true" : "false";
		res              = base::makeBox<state::TextComponent>(text, current_message_ids);
	}

	void EvaluateTemplateVisitor::visitMacroComponent(const template_file::MacroComponent& el) {
		if (auto macro = ctx.macros.atMaybe(el.macro); macro.has_value())
			(*macro.value())->acceptVisitor(*this);
		else
			CORE_PANIC("Macro '%s' not defined.", el.macro.c_str());
	}

	void EvaluateTemplateVisitor::visitCaseOfComponent(const template_file::CaseOfComponent& el) {
		// Evaluate pattern from template to state
		el.pattern->acceptVisitor(*this);

		// Convert state to text for pattern matching
		EvaluateToTextVisitor text_visitor;
		if (auto pattern_box = std::move(res).toOptBox(); pattern_box.has_value())
			(*pattern_box.value()).acceptVisitor(text_visitor);
		else
			CORE_PANIC("Pattern evaluation returned no component.");

		std::string pattern_value = text_visitor.result;

		// Find and evaluate matching case
		if (auto case_content = el.cases.atMaybe(pattern_value); case_content.has_value())
			(*case_content.value())->acceptVisitor(*this);
		else
			CORE_PANIC("No case matched for pattern value: %s", pattern_value.c_str());
	}

	void EvaluateTemplateVisitor::visitCodeBlockComponent(const template_file::CodeBlockComponent& el
	) {
		el.code_elements->acceptVisitor(*this);
		if (auto content = std::move(res).toOptBox(); content.has_value())
			res = base::makeBox<state::CodeBlockComponent>(std::move(content).value());
		else
			CORE_PANIC("CodeBlock evaluation returned no component.");
	}

	void EvaluateTemplateVisitor::visitMessageLinkComponent(
		const template_file::MessageLinkComponent& el
	) {
		// Evaluate target_message from template to state
		el.target_message->acceptVisitor(*this);

		// Convert state to text to get message ID
		EvaluateToTextVisitor text_visitor;
		if (auto message_box = std::move(res).toOptBox(); message_box.has_value())
			(*message_box.value()).acceptVisitor(text_visitor);
		else
			CORE_PANIC("Message link target evaluation returned no component.");

		std::string message_id_str = text_visitor.result;

		// Push message ID onto stack
		current_message_ids.push_back(static_cast<state::MessageID>(std::stoul(message_id_str)));

		// Evaluate content with the message ID in scope
		el.content->acceptVisitor(*this);

		// Pop message ID from stack
		current_message_ids.pop_back();
	}

	// ConvertDiagnosticFileVisitor methods
	void EvaluateDiagnosticFileVisitor::visitTextComponent(const dia_file::TextComponent& el) {
		res = base::makeBox<state::TextComponent>(el.content, current_message_ids);
	}

	void EvaluateDiagnosticFileVisitor::visitCodeComponent(const dia_file::CodeComponent& el) {
		res = base::makeBox<state::CodeComponent>(
			0,  // TODO: Generate proper component IDs
			el.content,
			current_pointer_message_ids,
			current_message_ids
		);
	}

	void EvaluateDiagnosticFileVisitor::visitCodeWithLocationComponent(
		const dia_file::CodeWithLocationComponent& el
	) {
		// For now, just extract the content
		el.content->acceptVisitor(*this);
	}

	void EvaluateDiagnosticFileVisitor::visitStartLineComponent(const dia_file::StartLineComponent& el
	) {
		res = base::makeBox<state::StartLineComponent>(el.number);
	}

	void EvaluateDiagnosticFileVisitor::visitConcatComponent(const dia_file::ConcatComponent& el) {
		usize initial_message_ids = current_message_ids.size();
		usize initial_pointer_ids = current_pointer_message_ids.size();

		std::vector<Box<state::Component>> components;
		for (const auto& child: el.elements) {
			child->acceptVisitor(*this);
			if (auto result = std::move(res).toOptBox(); result.has_value())
				components.push_back(std::move(result).value());
		}

		CORE_ASSERT(
			current_message_ids.size() == initial_message_ids, "Message IDs stack corrupted."
		);
		CORE_ASSERT(
			current_pointer_message_ids.size() == initial_pointer_ids,
			"Pointer message IDs stack corrupted."
		);

		res = base::makeBox<state::ConcatComponent>(std::move(components));
	}

	void EvaluateDiagnosticFileVisitor::visitPointedComponent(const dia_file::PointedComponent& el) {
		// Push pointer message IDs onto stack
		for (const auto& pm_id: el.pointer_messages) {
			current_pointer_message_ids.push_back(
				static_cast<state::PointerMessageID>(std::stoul(pm_id))
			);
		}

		// Evaluate content with pointer messages in scope
		el.content->acceptVisitor(*this);

		// Pop pointer message IDs from stack
		for (size_t i = 0; i < el.pointer_messages.size(); ++i)
			current_pointer_message_ids.pop_back();
	}

	void EvaluateDiagnosticFileVisitor::visitVariantComponent(const dia_file::VariantComponent& el) {
		// Evaluate both primary and alternative content
		EvaluateDiagnosticFileVisitor primary_converter(
			ctx, current_message_ids, current_pointer_message_ids
		);
		el.content->acceptVisitor(primary_converter);
		auto primary = std::move(primary_converter.res).toOptBox();

		EvaluateDiagnosticFileVisitor alt_converter(
			ctx, current_message_ids, current_pointer_message_ids
		);
		el.alt_content->acceptVisitor(alt_converter);
		auto alternative = std::move(alt_converter.res).toOptBox();

		if (primary.has_value() && alternative.has_value()) {
			res = base::makeBox<state::InteractiveComponent>(
				0,
				std::move(primary).value(),
				std::move(alternative).value()
			);
		} else {
			CORE_PANIC("Variant component evaluation failed.");
		}
	}

	void EvaluateDiagnosticFileVisitor::visitEntityComponent(const dia_file::EntityComponent& el) {
		// Push the attached messages to the entity to the current message stack.
		auto entity
			= ctx.entities.atMaybe(el.entity_id)
		          .expect(base::strConcat("Entity ", el.entity_id, " not found in context."));
		auto no_links = entity->assoc_infos.size();

		// Add assoc infos to message IDs stack
		for (const auto& assoc_info: entity->assoc_infos)
			current_message_ids.push_back(static_cast<state::MessageID>(std::stoul(assoc_info)));

		// Evaluate content with entity ID in scope
		el.content->acceptVisitor(*this);

		// Pop entity ID from stack
		for (size_t i = 0; i < no_links; ++i) current_message_ids.pop_back();
	}

	void EvaluateDiagnosticFileVisitor::visitTemplateComponent(const dia_file::EvaluatedTemplateComponent& el
	) {

		if (auto attached_msg = ctx.attached_messages.atMaybe(el.message_id);
		    attached_msg.has_value()) {
			const auto& msg = *attached_msg.value();

			// Load the template for this message
			auto& diagnostic_template = ctx.registry.loadTemplate(msg.metadata);

			// Apply template based on type
			std::visit(
				[&](auto&& tmpl) {
					using T = std::decay_t<decltype(tmpl)>;
					if constexpr (std::is_same_v<T, template_file::ComponentTemplate>) {
						// Evaluate component template
						MessageEvaluationContext component_ctx{
							ctx.registry, msg.params, {}, {}, {}
						};

						EvaluateTemplateVisitor visitor(component_ctx);
						visitor.current_message_ids         = current_message_ids;
						visitor.current_pointer_message_ids = current_pointer_message_ids;

						tmpl.content->acceptVisitor(visitor);
						res = std::move(visitor.res);

						current_message_ids = std::move(visitor.current_message_ids);
						current_pointer_message_ids
							= std::move(visitor.current_pointer_message_ids);
					} else {
						CORE_PANIC(
							"Template component reference points to non-component template: %s",
							el.message_id.c_str()
						);
					}
				},
				diagnostic_template.content
			);
		} else {
			CORE_PANIC(
				"Template component references unknown message ID: %s", el.message_id.c_str()
			);
		}
	}

	void EvaluateDiagnosticFileVisitor::visitMessageIDComponent(const dia_file::MessageIDComponent& el
	) {
		res = base::makeBox<state::TextComponent>(el.message_id);
	}

	// Free functions
	state::Message applyMessageTemplate(
		const template_file::MessageTemplate&                message_template,
		const dia_file::Message&                             message,
		TemplateRegistry&                                    registry,
		const base::HashMap<std::string, dia_file::Message>& attached_messages,
		const base::HashMap<std::string, dia_file::Entity>&  entities
	) {
		MessageEvaluationContext ctx(
			registry, message.params, message_template.macros, attached_messages, entities
		);

		// Evaluate header message
		EvaluateTemplateVisitor header_visitor(ctx);
		message_template.header_message->acceptVisitor(header_visitor);
		auto header = std::move(header_visitor.res).toOptBox();

		if (!header.has_value()) CORE_PANIC("Header message evaluation failed.");

		// Evaluate description if present
		std::vector<Box<state::Component>> description;
		if (message_template.description.toOpt().has_value()) {
			EvaluateTemplateVisitor desc_visitor(ctx);
			message_template.description->acceptVisitor(desc_visitor);
			if (auto desc_result = std::move(desc_visitor.res).toOptBox(); desc_result.has_value())
				description.push_back(std::move(desc_result).value());
		}

		return { message_template.metadata, std::move(header).value(), std::move(description) };
	}

	state::Thread applyTemplates(const dia_file::Thread& thread, const std::string& templates_root) {
		TemplateRegistry registry(templates_root);

		// Load the main message template
		auto& main_template = registry.loadTemplate(thread.main_message.metadata);

		// Process main message
		std::vector<state::Message> messages;
		std::visit(
			[&](auto&& tmpl) {
				using T = std::decay_t<decltype(tmpl)>;
				if constexpr (std::is_same_v<T, template_file::MessageTemplate>) {
					messages.push_back(applyMessageTemplate(
						tmpl, thread.main_message, registry, thread.attached_messages, thread.entities
					));
				} else {
					CORE_PANIC("Main message must use a MessageTemplate.");
				}
			},
			main_template.content
		);

		return { std::move(messages) };
	}
}
