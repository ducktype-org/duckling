#pragma once
#include "diagnostic_file.hpp"
#include "template_file.hpp"
#include "thread_state.hpp"

#include <base/box.hpp>

namespace dia_app {

	// Forward declarations
	class TemplateRegistry;
	class EvaluateToTextVisitor;
	class EvaluateTemplateVisitor;
	class EvaluateDiagnosticFileVisitor;

	/**
	 * @brief Registry for loading and caching diagnostic templates.
	 */
	class TemplateRegistry {
	private:
		base::HashMap<std::string, template_file::DiagnosticTemplate> cache;
		std::string                                                   templates_root;

	public:
		TemplateRegistry(std::string templates_root): templates_root(std::move(templates_root)) {}

		/**
		 * @brief Load a template by metadata.
		 */
		template_file::DiagnosticTemplate& loadTemplate(const dia_file::Metadata& metadata);
	};

	/**
	 * @brief Context for template evaluation with registry access.
	 */
	struct MessageEvaluationContext {
		TemplateRegistry&                                                registry;
		const base::HashMap<std::string, Box<dia_file::Component>>&      params;
		const base::HashMap<std::string, Box<template_file::Component>>& macros;
		const base::HashMap<std::string, dia_file::Message>&             attached_messages;
		const base::HashMap<std::string, dia_file::Entity>&              entities;

		MessageEvaluationContext(
			TemplateRegistry&                                                registry,
			const base::HashMap<std::string, Box<dia_file::Component>>&      params,
			const base::HashMap<std::string, Box<template_file::Component>>& macros,
			const base::HashMap<std::string, dia_file::Message>&             attached_messages,
			const base::HashMap<std::string, dia_file::Entity>&              entities
		):
			  registry(registry),
			  params(params),
			  macros(macros),
			  attached_messages(attached_messages),
			  entities(entities) {}
	};

	/**
	 * @brief Evaluate template component tree into state component tree.
	 *
	 * Accumulates message IDs and pointer message IDs along the way,
	 * copying them to leaf nodes (TextComponent and CodeComponent).
	 */
	class EvaluateTemplateVisitor: public template_file::ComponentVisitor {
	public:
		MBox<state::Component>               res;
		std::vector<state::MessageID>        current_message_ids;
		std::vector<state::PointerMessageID> current_pointer_message_ids;
		MessageEvaluationContext             ctx;

		EvaluateTemplateVisitor(MessageEvaluationContext ctx): ctx(ctx) {}

		void visitTextComponent(const template_file::TextComponent& el) override;
		void visitConcatComponent(const template_file::ConcatComponent& el) override;
		void visitParamComponent(const template_file::ParamComponent& el) override;
		void visitIsParamProvidedComponent(const template_file::IsParamProvidedComponent& el
		) override;
		void visitMacroComponent(const template_file::MacroComponent& el) override;
		void visitCaseOfComponent(const template_file::CaseOfComponent& el) override;
		void visitCodeBlockComponent(const template_file::CodeBlockComponent& el) override;
		void visitMessageLinkComponent(const template_file::MessageLinkComponent& el) override;
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
		void visitEvaluatedTemplateComponent(const dia_file::EvaluatedTemplateComponent& el) override;
		void visitMessageIDComponent(const dia_file::MessageIDComponent& el) override;
	};

	/**
	 * @brief Convert state components to plain text.
	 *
	 * Used for evaluating patterns in CaseOfComponent.
	 */
	class EvaluateToTextVisitor: public state::ComponentVisitor {
	public:
		std::string result;

		void visitTextComponent(const state::TextComponent& el) override;
		void visitCodeComponent(const state::CodeComponent& el) override;
		void visitConcatComponent(const state::ConcatComponent& el) override;
		void visitInteractiveComponent(const state::InteractiveComponent& el) override;
		void visitStartLineComponent(const state::StartLineComponent& el) override;
		void visitCodeBlockComponent(const state::CodeBlockComponent& el) override;
	};

	state::Message applyTemplate(
		const template_file::MessageTemplate&                message_template,
		const dia_file::Message&                             message,
		TemplateRegistry&                                    registry,
		const base::HashMap<std::string, dia_file::Message>* attached_messages = nullptr
	);

	state::Thread applyTemplates(const dia_file::Thread& thread, const std::string& templates_root);
}
