#include "template_evaluation.hpp"

#include "diagnostic_interactive/core/diagnostic_file.hpp"
#include "diagnostic_interactive/core/template_file.hpp"

#include <diagnostic_interactive/core/diagnostic_state.hpp>
#include <diagnostic_interactive/core/yaml_buffer.hpp>
#include <diagnostic_interactive/dia_templates/templates.hpp>

#include "base/raw_view.hpp"
#include "base/variant.hpp"
#include <base/str_utils.hpp>

#include <filesystem/file.hpp>

namespace dia_app {

	base::Optional<std::string_view> TemplateResistryEmbeddedProvider::loadTemplate(
		std::string_view path
	) {
		auto result = dia_embedded::loadTemplateFromPath(path);
		if (result.has_value()) return dia_embedded::loadTemplateFromPath(path).value();
		return {};
	}

	base::Optional<std::string_view> TemplateRegistryFilesystemProvider::loadTemplate(
		std::string_view path
	) {
		std::string path_str(path);
		auto        result = templates.atMaybe(path_str);
		if (result.has_value()) return std::string_view(*result.value());

		auto content_opt = fs::File(templates_root + path_str).getContentSafe();
		if (content_opt.has_value()) {
			std::string content_str(content_opt.value().view().stringView());
			templates.put(path_str, std::move(content_str));
			return std::string_view(*templates.atMaybe(path_str).value());
		}
		return {};
	}

	base::Optional<std::string_view> TemplateRegistryTestProvider::loadTemplate(std::string_view path
	) {
		return templates.atMaybe(std::string(path)).map([](Ref<std::string> str) {
			return std::string_view{ *str };
		});
	}

	struct ThreadEvaluationContext {
		TemplateRegistry&       registry;
		const dia_file::Thread& thread;

		base::HashMap<dia_file::MessageID, state::MessageID>               message_mapping;
		base::HashMap<dia_file::PointerMessageID, state::PointerMessageID> pointer_message_mapping;
	};

	/**
	 * @brief Context for template evaluation with registry access.
	 */
	struct MessageEvaluationContext {
		ThreadEvaluationContext& thread_ctx;

		// Template parameters and macros
		const base::HashMap<std::string, Box<dia_file::Component>>&      params;
		const base::HashMap<std::string, Box<template_file::Component>>& macros;
		const base::HashMap<dia_file::PointerMessageID, template_file::PointerMessage>&
			pointer_messages;

		// Diagnostic file

		// This is the evaluation result
		base::HashMap<state::PointerMessageID, state::PointerMessage> evaluated_pointer_messages;
		base::HashMap<dia_file::PointerMessageID, state::PointerMessageID> pointer_message_mapping;

		MessageEvaluationContext(
			ThreadEvaluationContext&                                         thread_ctx,
			const base::HashMap<std::string, Box<dia_file::Component>>&      params,
			const base::HashMap<std::string, Box<template_file::Component>>& macros,
			const base::HashMap<dia_file::PointerMessageID, template_file::PointerMessage>&
				pointer_messages
		):
			  thread_ctx(thread_ctx),
			  params(params),
			  macros(macros),
			  pointer_messages(pointer_messages) {}
	};

	/**
	 * @brief Evaluate template component tree into state component tree.
	 *
	 * Accumulates message IDs and pointer message IDs along the way,
	 * copying them to leaf nodes (TextComponent and CodeComponent).
	 */
	class EvaluateTemplateFileVisitor: public template_file::ComponentVisitor {
	public:
		MBox<state::Component>                res;
		MessageEvaluationContext&             ctx;
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

		static base::Optional<Box<state::Component>> evaluate(
			MessageEvaluationContext& ctx, CRef<template_file::Component> component
		) {
			std::vector<state::MessageID>        message_ids;
			std::vector<state::PointerMessageID> pointer_message_ids;
			EvaluateTemplateFileVisitor header_visitor(ctx, message_ids, pointer_message_ids);
			component->acceptVisitor(header_visitor);
			return std::move(header_visitor.res).toOptBox();
		}

		static base::Optional<Box<state::Component>> evaluateWithValues(
			MessageEvaluationContext              ctx,
			CRef<template_file::Component>        component,
			std::vector<state::MessageID>&        message_ids,
			std::vector<state::PointerMessageID>& pointer_message_ids
		) {
			EvaluateTemplateFileVisitor header_visitor(ctx, message_ids, pointer_message_ids);
			component->acceptVisitor(header_visitor);
			return std::move(header_visitor.res).toOptBox();
		}
	};

	/**
	 * @brief Convert diagnostic file components to state components.
	 *
	 * Handles accumulation of pointer messages and message IDs.
	 */
	class EvaluateDiagnosticFileVisitor: public dia_file::ComponentVisitor {
	public:
		MBox<state::Component>                res;
		MessageEvaluationContext              ctx;
		std::vector<state::MessageID>&        current_message_ids;
		std::vector<state::PointerMessageID>& current_pointer_message_ids;

		EvaluateDiagnosticFileVisitor(
			MessageEvaluationContext              ctx,
			std::vector<state::MessageID>&        message_ids,
			std::vector<state::PointerMessageID>& pointer_message_ids
		):
			  ctx(ctx),
			  current_message_ids(message_ids),
			  current_pointer_message_ids(pointer_message_ids) {}

		void visitTextComponent(const dia_file::TextComponent& el) override;
		void visitCodeComponent(const dia_file::CodeComponent& el) override;
		void visitCodeWithLocationComponent(const dia_file::CodeWithLocationComponent& el) override;
		void visitStartLineComponent(const dia_file::StartLineComponent& el) override;
		void visitConcatComponent(const dia_file::ConcatComponent& el) override;
		void visitPointedComponent(const dia_file::PointedComponent& el) override;
		void visitVariantComponent(const dia_file::VariantComponent& el) override;
		void visitEntityComponent(const dia_file::EntityComponent& el) override;
		void visitEvaluatedTemplateComponent(const dia_file::EvaluatedTemplateComponent& el
		) override;
		void visitMessageIDComponent(const dia_file::MessageIDComponent& el) override;
	};

	/**
	 * @brief Convert state components to plain text.
	 *
	 * Used for evaluating patterns in CaseOfComponent.
	 */
	class ConstructTextViewVisitor: public state::ComponentVisitorPanicky {
	public:
		std::string result;

		void visitTextComponent(const state::TextComponent& el) override;
		void visitCodeComponent(const state::CodeComponent& el) override;
		void visitConcatComponent(const state::ConcatComponent& el) override;
		void visitInteractiveComponent(const state::InteractiveComponent& el) override;
		void visitStartLineComponent(const state::StartLineComponent& el) override;
		void visitCodeBlockComponent(const state::CodeBlockComponent& el) override;
	};

	// EvaluateToTextVisitor methods

	void ConstructTextViewVisitor::visitTextComponent(const state::TextComponent& el) {
		result += el.content;
	}

	void ConstructTextViewVisitor::visitCodeComponent(const state::CodeComponent& el) {
		result += el.content;
	}

	void ConstructTextViewVisitor::visitConcatComponent(const state::ConcatComponent& el) {
		for (const auto& component: el.components) (*component).acceptVisitor(*this);
	}

	void ConstructTextViewVisitor::visitInteractiveComponent(const state::InteractiveComponent& el) {
		(*el.primary).acceptVisitor(*this);
	}

	void ConstructTextViewVisitor::visitStartLineComponent(const state::StartLineComponent& el) {
		// StartLineComponent doesn't produce visible text
		result += "\n";
	}

	void ConstructTextViewVisitor::visitCodeBlockComponent(const state::CodeBlockComponent& el) {
		(*el.content).acceptVisitor(*this);
	}

	bool checkMetadataMatch(
		const template_file::DiagnosticTemplate& tmpl, const dia_file::Metadata& metadata
	) {
		auto& template_metadata = tmpl.getMetadata();

		if (template_metadata.type != metadata.type) return false;
		if (template_metadata.family != metadata.family) return false;
		if (template_metadata.name != metadata.name) return false;

		variant_match(tmpl.content) {
			variant_case_novalue(template_file::MessageTemplate) {
				return metadata.template_type == "message";
			}
			variant_case_novalue(template_file::ComponentTemplate) {
				return metadata.template_type == "component";
			}
			variant_case_novalue(template_file::PointerMessageTemplate) {
				return metadata.template_type == "pointer_message";
			}
		}
		return false;
	}

	// TemplateRegistry methods
	template_file::DiagnosticTemplate& TemplateRegistry::loadTemplate(
		const dia_file::Metadata& metadata
	) {
		std::string key = metadata.type + "/" + metadata.family + "/" + metadata.name;

		if (auto cached = cache.atMaybe(key); cached.has_value()) return *cached.value();

		auto str_content_opt = provider->loadTemplate(key);
		if (!str_content_opt.has_value()) {
			CORE_PANIC(
				"Template not found: type='{}', family='{}', name='{}'.",
				metadata.type,
				metadata.family,
				metadata.name
			);
		}
		string_view_streambuf buf(str_content_opt.value());
		std::istream          is(&buf);
		YAML::Node            yaml_node = YAML::Load(is);

		auto diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml_node);

		CORE_ASSERT(
			checkMetadataMatch(diagnostic_template, metadata),
			"Loaded template metadata does not match requested metadata."
		);

		cache.put(key, std::move(diagnostic_template));
		return cache[key];
	}

	// EvaluateTemplateVisitor methods
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

	void EvaluateTemplateFileVisitor::visitParamComponent(const template_file::ParamComponent& el) {
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

	void EvaluateTemplateFileVisitor::visitIsParamProvidedComponent(
		const template_file::IsParamProvidedComponent& el
	) {
		std::string text = ctx.params.contains(el.param) ? "true" : "false";
		res              = base::makeBox<state::TextComponent>(text, current_message_ids);
	}

	void EvaluateTemplateFileVisitor::visitMacroComponent(const template_file::MacroComponent& el) {
		if (auto macro = ctx.macros.atMaybe(el.macro); macro.has_value())
			(*macro.value())->acceptVisitor(*this);
		else
			CORE_PANIC("Macro '%s' not defined.", el.macro.c_str());
	}

	void EvaluateTemplateFileVisitor::visitCaseOfComponent(const template_file::CaseOfComponent& el
	) {
		// Evaluate pattern from template to state
		el.pattern->acceptVisitor(*this);

		// Convert state to text for pattern matching
		ConstructTextViewVisitor text_visitor;
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

	void EvaluateTemplateFileVisitor::visitCodeBlockComponent(
		const template_file::CodeBlockComponent& el
	) {
		el.code_elements->acceptVisitor(*this);
		if (auto content = std::move(res).toOptBox(); content.has_value())
			res = base::makeBox<state::CodeBlockComponent>(std::move(content).value());
		else
			CORE_PANIC("CodeBlock evaluation returned no component.");
	}

	void EvaluateTemplateFileVisitor::visitMessageLinkComponent(
		const template_file::MessageLinkComponent& el
	) {
		// Evaluate target_message from template to state
		el.target_message->acceptVisitor(*this);

		// Convert state to text to get message ID
		ConstructTextViewVisitor text_visitor;
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
				static_cast<state::PointerMessageID>(std::stoul(pm_id.pointer_message_id))
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
				0, std::move(primary).value(), std::move(alternative).value()
			);
		} else {
			CORE_PANIC("Variant component evaluation failed.");
		}
	}

	void EvaluateDiagnosticFileVisitor::visitEntityComponent(const dia_file::EntityComponent& el) {
		// Push the attached messages to the entity to the current message stack.
		auto entity
			= ctx.thread.entities.atMaybe(el.entity_id)
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

	void EvaluateDiagnosticFileVisitor::visitEvaluatedTemplateComponent(
		const dia_file::EvaluatedTemplateComponent& el
	) {
		if (auto attached_msg = ctx.thread.attached_messages.atMaybe(el.message_id);
		    attached_msg.has_value()) {
			const auto& msg = *attached_msg.value();

			// Load the template for this message
			auto& diagnostic_template = ctx.registry.loadTemplate(msg.metadata);

			// Apply template based on type
			std::visit(
				[&](auto&& tmpl) {
					using T = std::decay_t<decltype(tmpl)>;
					if constexpr (std::is_same_v<T, template_file::ComponentTemplate>) {
						// New evalution context should have the parameters from the message
						MessageEvaluationContext component_ctx{
							ctx.registry, msg.params, tmpl.macros, {}, {}
						};

						auto result = EvaluateTemplateFileVisitor::evaluateWithValues(
							component_ctx,
							tmpl.content.ref(),
							current_message_ids,
							current_pointer_message_ids
						);

						if (result.has_value())
							res = std::move(result).value();
						else
							CORE_PANIC("Template component evaluation returned no component.");
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
		CORE_ASSERT(
			ctx.thread.attached_messages.contains(el.message_id),
			"Message ID not found in attached messages."
		);
	}

	// Free functions
	state::Message evaluateMessage(
		ThreadEvaluationContext&              thread_ctx,
		const template_file::MessageTemplate& message_template,
		const dia_file::Message&              message,
	) {
		MessageEvaluationContext ctx(
			thread_ctx, message.params, message_template.macros, message_template.pointer_messages
		);

		// Evaluate header message
		auto header
			= EvaluateTemplateFileVisitor::evaluate(ctx, message_template.header_message.ref());
		if (!header.has_value()) CORE_PANIC("Header message evaluation failed.");

		// Evaluate description if present
		base::MBox<state::Component> description;
		if (message_template.description.ref().toOpt().has_value()) {
			auto desc = EvaluateTemplateFileVisitor::evaluate(
				ctx, message_template.description.ref().toOpt().value()
			);
			if (!desc.has_value()) CORE_PANIC("Description message evaluation failed.");
			description = base::MBox<state::Component>(std::move(desc).value());
		}

		return { message_template.metadata, std::move(header).value(), std::move(description) };
	}

	base::Optional<state::Message> evaluateTemplate(
		ThreadEvaluationContext&                 thread_ctx,
		const template_file::DiagnosticTemplate& generic_template,
		const dia_file::Message&                 message
	) {
		return std::visit(
			[&](auto&& tmpl) -> base::Optional<state::Message> {
				using T = std::decay_t<decltype(tmpl)>;
				if constexpr (std::is_same_v<T, template_file::MessageTemplate>)
					return evaluateMessage(thread_ctx, tmpl, message);
				else
					return {};
			},
			generic_template.content
		);
	}

	state::Diagnostic evaluateDiagnostic(const dia_file::Thread& thread) {
		TemplateRegistry& registry = TemplateRegistry::getInstance();


		ThreadEvaluationContext thread_ctx{ .registry = registry,
			                                .thread   = thread,
			                                .message_mapping
			                                = { { "<<main_message_not_used_id>>", 0 } },
			                                .pointer_message_mapping = {} };

		state::MessageID message_id_generator = 1;
		for (const auto&& [message_id, message]: thread.additional_messages)
			if (message.metadata.template_type == "message")
				thread_ctx.message_mapping.insert_or_assign(message_id, message_id_generator++);

		std::vector<state::Message> messages;
		// Process main message

		auto& main_template = registry.loadTemplate(thread.main_message.metadata);
		messages.push_back(evaluateTemplate(thread_ctx, main_template, thread.main_message)
		                       .expect("The template is not a message template."));

		for (const auto& [message_id, message]: thread.additional_messages) {
			messages.push_back(
				evaluateTemplate(thread_ctx, registry.loadTemplate(message.metadata), message)
					.expect("The template is not a message template.")
			);
		}


		// Load the main message template
		std::vector<state::MessageID> displayed_messages = { 0 };
		for (auto&& msg_id: thread.main_message.attached_messages)
			displayed_messages.push_back(thread_ctx.message_mapping.at(msg_id));


		return { std::move(displayed_messages), std::move(messages) };
	}

	TemplateRegistry& TemplateRegistry::getInstance() { return *instance.toOpt().value(); }

	void TemplateRegistry::setInstance(Box<TemplateRegistryProvider> provider) {
		instance = base::makeBox<TemplateRegistry>(std::move(provider));
	}

	std::string constructTextView(CRef<state::Component> component) {
		ConstructTextViewVisitor text_visitor;
		component->acceptVisitor(text_visitor);
		return text_visitor.result;
	}
}
