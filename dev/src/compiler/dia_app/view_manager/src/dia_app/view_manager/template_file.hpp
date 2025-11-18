#pragma once

#include "utils.hpp"

#include <yaml-cpp/yaml.h>

#include <base/box.hpp>
#include <base/visitor.hpp>

#include <cctype>
#include <limits>

namespace dia_app::template_file {

	// Forward declarations of template elements and a template visitor.

	struct Component;

	struct TextComponent;
	struct ConcatComponent;
	struct ParamComponent;
	struct IsParamProvidedComponent;
	struct MacroComponent;
	struct CaseOfComponent;
	struct CodeBlockComponent;

	MAKE_VISITOR(Component,
			TextComponent,
			ConcatComponent,
			ParamComponent,
            IsParamProvidedComponent,
			MacroComponent,
			CaseOfComponent,
            CodeBlockComponent
		);

	/**
	 * @brief A base class template element.
	 *
	 * A template element represents a message fragment of an info template
	 * file. It can define the text to be displayed, additional metadata,
	 * or act as a logical branching.
	 *
	 * The only required method of each template element is `accept`.
	 *
	 */
	struct Component {
		virtual ~Component() {}

		virtual void acceptVisitor(ComponentVisitor& visitor) = 0;

		static Box<Component> fromYaml(const YAML::Node& elem) {
			if (elem.IsScalar()) return base::makeBox<TextComponent>(elem.as<std::string>());
			if (elem.IsSequence()) return base::makeBox<ConcatComponent>(elem);
			if (elem["param"]) return base::makeBox<ParamComponent>(elem);
			if (elem["is_param_provided"]) return base::makeBox<IsParamProvidedComponent>(elem);
			if (elem["macro"]) return base::makeBox<MacroComponent>(elem);
			if (elem["case"]) return base::makeBox<CaseOfComponent>(elem);
			if (elem["codeblock"]) return base::makeBox<CodeBlockComponent>(elem);

			CORE_PANIC("Unknown template component.");
		}
	};

	/**
	 * @brief A template element representing simple text.
	 *
	 */
	struct TextComponent: public Component {
		std::string text;

		TextComponent(std::string text): text(std::move(text)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitTextComponent(*this); }
	};

	/**
	 * @brief A template element representing a concatenation
	 * of other template elements.
	 *
	 */
	struct ConcatComponent: public Component {
		std::vector<Box<Component>> elements;

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitConcatComponent(*this); }

		static Box<ConcatComponent> fromYaml(const YAML::Node& elem_node) {
			CORE_ASSERT(elem_node.IsSequence(), "Concat component must be a sequence.");

			auto result = base::makeBox<ConcatComponent>();
			for (const auto& el: elem_node) result->elements.push_back(Component::fromYaml(el));
			return result;
		}
	};

	/**
	 * @brief A template element representing a parameter passed down
	 * by the compiler.
	 *
	 */
	struct ParamComponent: public Component {
		std::string param;

		ParamComponent(std::string param): param(std::move(param)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitParamComponent(*this); }

		static Box<ParamComponent> fromYaml(const YAML::Node& elem_node) {
			YAML_ASSUME_HAS_SCALAR(elem_node, "param");
			return base::makeBox<ParamComponent>(elem_node["param"].as<std::string>());
		}
	};

	/**
	 * @brief A template element representing an expansion of
	 * a template macro.
	 *
	 */
	struct MacroComponent: public Component {
		std::string macro;

		MacroComponent(std::string macro): macro(std::move(macro)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitMacroComponent(*this); }

		static Box<MacroComponent> fromYaml(const YAML::Node& elem_node) {
			YAML_ASSUME_HAS_SCALAR(elem_node, "macro");
			return base::makeBox<MacroComponent>(elem_node["macro"].as<std::string>());
		}
	};

	/**
	 * @brief A template element representing a logical branching
	 * of template evaluation based on the provided pattern.
	 *
	 */
	struct CaseOfComponent: public Component {
		Box<Component>                         pattern;
		base::Map<std::string, Box<Component>> cases;

		CaseOfComponent(Box<Component> pattern, base::Map<std::string, Box<Component>> cases):
			  pattern(std::move(pattern)),
			  cases(std::move(cases)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitCaseOfComponent(*this); }

		static Box<CaseOfComponent> fromYaml(const YAML::Node& elem_node) {
			YAML_ASSUME_HAS(elem_node, "case");
			CORE_ASSERT(
				elem_node["of"] && elem_node["of"].IsMap(),
				"CaseOf component must have an 'of' field of map type."
			);

			auto                                   pattern = Component::fromYaml(elem_node["case"]);
			base::Map<std::string, Box<Component>> cases;
			for (const auto& it: elem_node["of"]) {
				const auto key = it.first.as<std::string>();
				cases.put(key, Component::fromYaml(it.second));
			}
			return base::makeBox<CaseOfComponent>(std::move(pattern), std::move(cases));
		}
	};

	struct CodeBlockComponent: public Component {
		Box<Component> code_elements;

		CodeBlockComponent(Box<Component> code_elements): code_elements(std::move(code_elements)) {}

		void acceptVisitor(ComponentVisitor& visitor) final {
			visitor.visitCodeBlockComponent(*this);
		}

		static Box<CodeBlockComponent> fromYaml(const YAML::Node& elem_node) {
			YAML_ASSUME_HAS(elem_node, "codeblock");
			auto code_elements = Component::fromYaml(elem_node["codeblock"]);
			return base::makeBox<CodeBlockComponent>(std::move(code_elements));
		}
	};

	struct Metadata {
		enum class TemplateType { Message, Component, PointerMessage };

		TemplateType template_type;
		std::string  type;
		std::string  family;
		std::string  name;

		int         code;
		std::string active_from;
		std::string active_until;

		Metadata(
			TemplateType template_type,
			std::string  type,
			std::string  family,
			std::string  name,
			int          code,
			std::string  active_from,
			std::string  active_until
		):
			  template_type(template_type),
			  type(std::move(type)),
			  family(std::move(family)),
			  name(std::move(name)),
			  code(code),
			  active_from(std::move(active_from)),
			  active_until(std::move(active_until)) {}

		static Metadata fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Metadata definition must be a map.");

			YAML_ASSUME_HAS_SCALAR(node, "template_type");
			YAML_ASSUME_HAS_SCALAR(node, "type");
			YAML_ASSUME_HAS_SCALAR(node, "family");
			YAML_ASSUME_HAS_SCALAR(node, "name");
			YAML_ASSUME_HAS_SCALAR(node, "code");
			YAML_ASSUME_HAS_SCALAR(node, "active_from");
			YAML_ASSUME_HAS_SCALAR(node, "active_until");

			std::string  template_type_str = node["template_type"].as<std::string>();
			TemplateType template_type     = TemplateType::Message;  // Default value
			if (template_type_str == "message")
				template_type = TemplateType::Message;
			else if (template_type_str == "component")
				template_type = TemplateType::Component;
			else if (template_type_str == "pointer_message")
				template_type = TemplateType::PointerMessage;
			else
				CORE_PANIC("Unknown template_type in metadata: %s", template_type_str.c_str());

			return Metadata(
				template_type,
				node["type"].as<std::string>(),
				node["family"].as<std::string>(),
				node["name"].as<std::string>(),
				node["code"].as<int>(),
				node["active_from"].as<std::string>(),
				node["active_until"].as<std::string>()
			);
		}
	};

	struct Parameter {
		std::string type;
		std::string description;
		bool        is_required;

		Parameter(std::string type, std::string description, bool is_required):
			  type(std::move(type)),
			  description(std::move(description)),
			  is_required(is_required) {}

		static Parameter fromYaml(const std::string& name, const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Parameter definition must be a map.");
			YAML_ASSUME_HAS_SCALAR(node, "description");
			YAML_ASSUME_HAS_SCALAR(node, "type");

			return Parameter(
				node["type"].as<std::string>(),
				node["description"].as<std::string>(),
				not(node["optional"] && node["optional"].as<bool>())
			);
		}
	};

	struct Edge {
		Box<Component>                        content;
		base::HashMap<std::string, Parameter> params;

		Edge(Box<Component> content, base::HashMap<std::string, Parameter> params):
			  content(std::move(content)),
			  params(std::move(params)) {}

		static Edge fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Explore edge definition must be a map.");
			YAML_ASSUME_HAS(node, "content");
			YAML_ASSUME_HAS(node, "params");

			auto content    = Component::fromYaml(node["content"]);
			auto params_map = yamlToMap<Parameter>(node, "params");
			return { std::move(content), std::move(params_map) };
		}
	};

	struct PointerMessage {
		std::string    type;
		int            priority;
		Box<Component> content;

		PointerMessage(std::string type, int priority, Box<Component> content):
			  type(std::move(type)),
			  priority(priority),
			  content(std::move(content)) {}

		static PointerMessage fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Pointer message definition must be a map.");
			YAML_ASSUME_HAS(node, "content");
			YAML_ASSUME_HAS_SCALAR(node, "type");

			auto content = Component::fromYaml(node["content"]);
			int  priority
				= node["priority"] ? node["priority"].as<int>() : std::numeric_limits<int>::max();
			return PointerMessage(node["type"].as<std::string>(), priority, std::move(content));
		}
	};

	struct CommonTemplate {
		Metadata                                   metadata;
		base::HashMap<std::string, Parameter>      params;
		base::HashMap<std::string, Box<Component>> macros;

		CommonTemplate(
			Metadata                                   metadata,
			base::HashMap<std::string, Parameter>      params,
			base::HashMap<std::string, Box<Component>> macros
		):
			  metadata(std::move(metadata)),
			  params(std::move(params)),
			  macros(std::move(macros)) {}

		static CommonTemplate fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Template definition must be a map.");
			YAML_ASSUME_HAS(node, "metadata");

			auto metadata = Metadata::fromYaml(node["metadata"]);
			auto params   = yamlToMap<Parameter>(node, "params");

			base::HashMap<std::string, Box<Component>> macros;
			if (const auto macros_node = node["macros"]; macros_node && macros_node.IsMap())
				macros = yamlToBoxMap<Component>(node, "macros");

			return { std::move(metadata), std::move(params), std::move(macros) };
		}
	};

	struct MessageTemplate: public CommonTemplate {
		Box<Component>                             header_message;
		base::MBox<Component>                      description;
		base::HashMap<std::string, Edge>           explore_edges;
		base::HashMap<std::string, PointerMessage> pointer_messages;

		MessageTemplate(
			CommonTemplate                             common,
			Box<Component>                             header_message,
			base::MBox<Component>                      description,
			base::HashMap<std::string, Edge>           explore_edges,
			base::HashMap<std::string, PointerMessage> pointer_messages
		):
			  CommonTemplate(std::move(common)),
			  header_message(std::move(header_message)),
			  description(std::move(description)),
			  explore_edges(std::move(explore_edges)),
			  pointer_messages(std::move(pointer_messages)) {}

		static MessageTemplate fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Message template must be a map.");
			auto common = CommonTemplate::fromYaml(node);
			YAML_ASSUME_HAS(node, "header_message");

			auto                  header = Component::fromYaml(node["header_message"]);
			base::MBox<Component> description;
			if (node["description"])
				description = base::MBox<Component>(Component::fromYaml(node["description"]));

			auto edges_map   = yamlToMap<Edge>(node, "explore_edges");
			auto pointer_map = yamlToMap<PointerMessage>(node, "pointer_messages");

			return { std::move(common),
				     std::move(header),
				     std::move(description),
				     std::move(edges_map),
				     std::move(pointer_map) };
		}
	};

	struct ComponentTemplate: public CommonTemplate {
		Box<Component> content;

		ComponentTemplate(CommonTemplate common, Box<Component> content):
			  CommonTemplate(std::move(common)),
			  content(std::move(content)) {}

		static ComponentTemplate fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Component template must be a map.");
			auto common = CommonTemplate::fromYaml(node);
			YAML_ASSUME_HAS(node, "content");
			auto content = Component::fromYaml(node["content"]);
			return { std::move(common), std::move(content) };
		}
	};

	struct PointerMessageTemplate: public CommonTemplate {
		base::HashMap<std::string, PointerMessage> pointer_messages;

		PointerMessageTemplate(
			CommonTemplate common, base::HashMap<std::string, PointerMessage> pointer_messages
		):
			  CommonTemplate(std::move(common)),
			  pointer_messages(std::move(pointer_messages)) {}

		static PointerMessageTemplate fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Pointer message template must be a map.");
			auto common      = CommonTemplate::fromYaml(node);
			auto pointer_map = yamlToMap<PointerMessage>(node, "pointer_messages");
			return { std::move(common), std::move(pointer_map) };
		}
	};

	struct DiagnosticTemplate {
		std::variant<MessageTemplate, ComponentTemplate, PointerMessageTemplate> content;

		static DiagnosticTemplate fromYaml(const YAML::Node& node) {
			CORE_ASSERT(node && node.IsMap(), "Diagnostic template must be a map.");
			YAML_ASSUME_HAS(node, "metadata");

			auto metadata = Metadata::fromYaml(node["metadata"]);
			switch (metadata.template_type) {
			case Metadata::TemplateType::Message: {
				auto message_template = MessageTemplate::fromYaml(node);
				return DiagnosticTemplate{ std::move(message_template) };
			}
			case Metadata::TemplateType::Component: {
				auto component_template = ComponentTemplate::fromYaml(node);
				return DiagnosticTemplate{ std::move(component_template) };
			}
			case Metadata::TemplateType::PointerMessage: {
				auto pointer_template = PointerMessageTemplate::fromYaml(node);
				return DiagnosticTemplate{ std::move(pointer_template) };
			}
			default:
				CORE_PANIC("Unknown template type in diagnostic template.");
			}
		}
	};
}
