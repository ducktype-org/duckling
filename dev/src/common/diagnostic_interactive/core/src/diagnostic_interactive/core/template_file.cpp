#include "template_file.hpp"

namespace dia_app::template_file {
	Box<Component> Component::fromYaml(const YAML::Node& elem) {
		if (elem.IsScalar()) return TextComponent::fromYaml(elem);
		if (elem.IsSequence()) return ConcatComponent::fromYaml(elem);
		if (elem["param"]) return ParamComponent::fromYaml(elem);
		if (elem["is_param_provided"]) return IsParamProvidedComponent::fromYaml(elem);
		if (elem["macro"]) return MacroComponent::fromYaml(elem);
		if (elem["case"]) return CaseOfComponent::fromYaml(elem);
		if (elem["codeblock"]) return CodeBlockComponent::fromYaml(elem);
		if (elem["url"]) return MessageLinkComponent::fromYaml(elem);

		CORE_PANIC("Unknown template component.");
	}

	Box<ConcatComponent> ConcatComponent::fromYaml(const YAML::Node& elem_node
	) {
		CORE_ASSERT(elem_node.IsSequence(), "Concat component must be a sequence.");
		std::vector<Box<Component>> elements;
		for (const auto& el: elem_node) elements.push_back(Component::fromYaml(el));
		return base::makeBox<ConcatComponent>(std::move(elements));
	}

	Box<ParamComponent> ParamComponent::fromYaml(const YAML::Node& elem_node
	) {
		YAML_ASSUME_HAS_SCALAR(elem_node, "param");
		return base::makeBox<ParamComponent>(elem_node["param"].as<std::string>());
	}

	Box<IsParamProvidedComponent> IsParamProvidedComponent::fromYaml(
		const YAML::Node& elem_node
	) {
		YAML_ASSUME_HAS_SCALAR(elem_node, "is_param_provided");
		return base::makeBox<IsParamProvidedComponent>(
			elem_node["is_param_provided"].as<std::string>()
		);
	}

	Box<MacroComponent> MacroComponent::fromYaml(const YAML::Node& elem_node
	) {
		YAML_ASSUME_HAS_SCALAR(elem_node, "macro");
		return base::makeBox<MacroComponent>(elem_node["macro"].as<std::string>());
	}

	Box<CaseOfComponent> CaseOfComponent::fromYaml(const YAML::Node& elem_node
	) {
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

	Box<CodeBlockComponent> CodeBlockComponent::fromYaml(
		const YAML::Node& elem_node
	) {
		YAML_ASSUME_HAS(elem_node, "codeblock");
		auto code_elements = Component::fromYaml(elem_node["codeblock"]);
		return base::makeBox<CodeBlockComponent>(std::move(code_elements));
	}

	Box<MessageLinkComponent> MessageLinkComponent::fromYaml(
		const YAML::Node& elem_node
	) {
		YAML_ASSUME_HAS(elem_node, "content");
		YAML_ASSUME_HAS(elem_node, "url");
		auto content = Component::fromYaml(elem_node["content"]);
		auto url     = Component::fromYaml(elem_node["url"]);
		return base::makeBox<MessageLinkComponent>(std::move(content), std::move(url));
	}

	Metadata Metadata::fromYaml(const YAML::Node& node
	) {
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

	Parameter Parameter::fromYaml(
		const YAML::Node& node
	) {
		CORE_ASSERT(node && node.IsMap(), "Parameter definition must be a map.");
		YAML_ASSUME_HAS_SCALAR(node, "description");

		base::Optional<std::string> component_type;
		if (node["component_type"]) component_type = node["component_type"].as<std::string>();

		bool optional = false;
		if (node["optional"]) optional = node["optional"].as<bool>();

		return { node["description"].as<std::string>(), std::move(component_type), optional };
	}

	Edge Edge::fromYaml(const YAML::Node& node) {
		CORE_ASSERT(node && node.IsMap(), "Explore edge definition must be a map.");
		YAML_ASSUME_HAS(node, "content");
		YAML_ASSUME_HAS(node, "params");

		auto content    = Component::fromYaml(node["content"]);
		auto params_map = yamlToMap<Parameter>(node, "params");
		return Edge(std::move(content), std::move(params_map));
	}

	PointerMessage PointerMessage::fromYaml(
		const YAML::Node& node
	) {
		CORE_ASSERT(node && node.IsMap(), "Pointer message definition must be a map.");
		YAML_ASSUME_HAS(node, "content");
		YAML_ASSUME_HAS_SCALAR(node, "type");

		auto content = Component::fromYaml(node["content"]);
		int  priority
			= node["priority"] ? node["priority"].as<int>() : std::numeric_limits<int>::max();
		return { node["type"].as<std::string>(), priority, std::move(content) };
	}

	CommonTemplate CommonTemplate::fromYaml(
		const YAML::Node& node
	) {
		CORE_ASSERT(node && node.IsMap(), "Template definition must be a map.");
		YAML_ASSUME_HAS(node, "metadata");

		auto metadata = Metadata::fromYaml(node["metadata"]);
		auto params   = yamlToMap<Parameter>(node, "params");

		base::HashMap<std::string, Box<Component>> macros;
		if (const auto macros_node = node["macros"]; macros_node && macros_node.IsMap())
			macros = yamlToBoxMap<Component>(node, "macros");

		return { std::move(metadata), std::move(params), std::move(macros) };
	}

	MessageTemplate MessageTemplate::fromYaml(const YAML::Node& node) {
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

	ComponentTemplate ComponentTemplate::fromYaml(const YAML::Node& node) {
		CORE_ASSERT(node && node.IsMap(), "Component template must be a map.");
		auto common = CommonTemplate::fromYaml(node);
		YAML_ASSUME_HAS(node, "content");
		auto content = Component::fromYaml(node["content"]);
		return { std::move(common), std::move(content) };
	}

	PointerMessageTemplate PointerMessageTemplate::fromYaml(const YAML::Node& node) {
		CORE_ASSERT(node && node.IsMap(), "Pointer message template must be a map.");
		auto common      = CommonTemplate::fromYaml(node);
		auto pointer_map = yamlToMap<PointerMessage>(node, "pointer_messages");
		return { std::move(common), std::move(pointer_map) };
	}

	DiagnosticTemplate DiagnosticTemplate::fromYaml(const YAML::Node& node) {
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
}
