/**
 * @file template_file.hpp
 * @author Wojciech Rzeplinski
 * @brief Implementation of diagnostic template file elements.
 * The logic for deserializing template files into element trees is here.
 *
 */
#pragma once

#include <yaml-cpp/yaml.h>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/visitor.hpp>
#include <base/pointers/box.hpp>

namespace dia::template_file {

	// Forward declarations of template elements and a template visitor.

	struct Component;

	struct TextComponent;
	struct ConcatComponent;
	struct ParamComponent;
	struct IsParamProvidedComponent;
	struct MacroComponent;
	struct CaseOfComponent;
	struct CodeBlockComponent;
	struct MessageLinkComponent;
	struct VariantComponent;

	MAKE_VISITOR(Component,
			TextComponent,
			ConcatComponent,
			ParamComponent,
            IsParamProvidedComponent,
			MacroComponent,
			CaseOfComponent,
            CodeBlockComponent,
            MessageLinkComponent,
            VariantComponent
		);

	/**
	 * @brief A base class template element.
	 *
	 * A template element represents a message fragment of an info template
	 * file. It can define the text to be displayed, additional metadata,
	 * or act as a logical branching.
	 *
	 * The only required method of each template element is `accept`.
	 */
	struct Component {
		virtual ~Component() = default;

		virtual void acceptVisitor(ComponentVisitor& visitor) const = 0;

		static Box<Component> fromYaml(const YAML::Node& elem);
	};

	/**
	 * @brief A template element representing simple text.
	 *
	 */
	struct TextComponent final: public Component {
		std::string text;

		TextComponent(std::string text): text(std::move(text)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitTextComponent(*this);
		}

		static Box<TextComponent> fromYaml(const YAML::Node& elem_node);
	};

	/**
	 * @brief A template element representing a concatenation
	 * of other template elements.
	 *
	 */
	struct ConcatComponent final: public Component {
		std::vector<Box<Component>> elements;

		ConcatComponent() = default;

		ConcatComponent(std::vector<Box<Component>> elements): elements(std::move(elements)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitConcatComponent(*this);
		}

		static Box<ConcatComponent> fromYaml(const YAML::Node& elem_node);
	};

	/**
	 * @brief A template element representing a parameter passed down
	 * by the compiler.
	 *
	 */
	struct ParamComponent final: public Component {
		std::string param;

		ParamComponent(std::string param): param(std::move(param)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitParamComponent(*this);
		}

		static Box<ParamComponent> fromYaml(const YAML::Node& elem_node);
	};

	/**
	 * @brief A template element for conditional logic based on parameter existence.
	 *
	 */
	struct IsParamProvidedComponent final: public Component {
		std::string param;

		IsParamProvidedComponent(std::string param): param(std::move(param)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitIsParamProvidedComponent(*this);
		}

		static Box<IsParamProvidedComponent> fromYaml(const YAML::Node& elem_node);
	};

	/**
	 * @brief A template element representing an expansion of
	 * a template macro.
	 *
	 */
	struct MacroComponent final: public Component {
		std::string macro;

		MacroComponent(std::string macro): macro(std::move(macro)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitMacroComponent(*this);
		}

		static Box<MacroComponent> fromYaml(const YAML::Node& elem_node);
	};

	/**
	 * @brief A template element representing a logical branching
	 * of template evaluation based on the provided pattern.
	 *
	 */
	struct CaseOfComponent final: public Component {
		Box<Component>                         pattern;
		base::Map<std::string, Box<Component>> cases;

		CaseOfComponent(Box<Component> pattern, base::Map<std::string, Box<Component>> cases):
			  pattern(std::move(pattern)),
			  cases(std::move(cases)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCaseOfComponent(*this);
		}

		static Box<CaseOfComponent> fromYaml(const YAML::Node& elem_node);
	};

	struct CodeBlockComponent final: public Component {
		Box<Component>        code_elements;
		base::MBox<Component> location;

		CodeBlockComponent(Box<Component> code_elements, base::MBox<Component> location):
			  code_elements(std::move(code_elements)),
			  location(std::move(location)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeBlockComponent(*this);
		}

		static Box<CodeBlockComponent> fromYaml(const YAML::Node& elem_node);
	};

	struct MessageLinkComponent final: public Component {
		Box<Component> content;
		Box<Component> target_message;

		MessageLinkComponent(Box<Component> content, Box<Component> url):
			  content(std::move(content)),
			  target_message(std::move(url)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitMessageLinkComponent(*this);
		}

		static Box<MessageLinkComponent> fromYaml(const YAML::Node& elem_node);
	};

	struct VariantComponent final: public Component {
		Box<Component> content;
		Box<Component> alt_content;

		VariantComponent(Box<Component> content, Box<Component> alt_content):
			  content(std::move(content)),
			  alt_content(std::move(alt_content)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitVariantComponent(*this);
		}

		static Box<VariantComponent> fromYaml(const YAML::Node& elem_node);
	};

	struct Metadata final {
		enum class TemplateType { Message, Component, PointerMessage };

		TemplateType template_type;
		std::string  type;
		std::string  family;
		std::string  name;

		u64         code;
		std::string active_from;
		std::string active_until;

		Metadata(
			TemplateType template_type,
			std::string  type,
			std::string  family,
			std::string  name,
			u64          code,
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

		static Metadata fromYaml(const YAML::Node& node);
	};

	struct Parameter final {
		std::string description;
		/**
		 * Expected component type
		 */
		base::Optional<std::string> argument_expected_type;
		bool                        optional;

		Parameter(std::string description, base::Optional<std::string> component_type, bool optional):
			  description(std::move(description)),
			  argument_expected_type(std::move(component_type)),
			  optional(optional) {}

		static Parameter fromYaml(const YAML::Node& node);
	};

	struct ExploreLink final {
		Box<Component>                        content;
		base::HashMap<std::string, Parameter> params;

		ExploreLink(Box<Component> content, base::HashMap<std::string, Parameter> params):
			  content(std::move(content)),
			  params(std::move(params)) {}

		static ExploreLink fromYaml(const YAML::Node& node);
	};

	struct PointerMessage final {
		std::string    type;
		int            priority;
		Box<Component> content;

		PointerMessage(std::string type, int priority, Box<Component> content):
			  type(std::move(type)),
			  priority(priority),
			  content(std::move(content)) {}

		static PointerMessage fromYaml(const YAML::Node& node);
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

		static CommonTemplate fromYaml(const YAML::Node& node);
	};

	struct MessageTemplate: public CommonTemplate {
		Box<Component>                             header_message;
		base::MBox<Component>                      description;
		base::HashMap<std::string, ExploreLink>    explore_links;
		base::HashMap<std::string, PointerMessage> pointer_messages;

		MessageTemplate(
			CommonTemplate                             common,
			Box<Component>                             header_message,
			base::MBox<Component>                      description,
			base::HashMap<std::string, ExploreLink>    explore_links,
			base::HashMap<std::string, PointerMessage> pointer_messages
		):
			  CommonTemplate(std::move(common)),
			  header_message(std::move(header_message)),
			  description(std::move(description)),
			  explore_links(std::move(explore_links)),
			  pointer_messages(std::move(pointer_messages)) {}

		static MessageTemplate fromYaml(const YAML::Node& node);
	};

	struct ComponentTemplate: public CommonTemplate {
		Box<Component> content;

		ComponentTemplate(CommonTemplate common, Box<Component> content):
			  CommonTemplate(std::move(common)),
			  content(std::move(content)) {}

		static ComponentTemplate fromYaml(const YAML::Node& node);
	};

	struct PointerMessageTemplate: public CommonTemplate {
		base::HashMap<std::string, PointerMessage> pointer_messages;

		PointerMessageTemplate(
			CommonTemplate common, base::HashMap<std::string, PointerMessage> pointer_messages
		):
			  CommonTemplate(std::move(common)),
			  pointer_messages(std::move(pointer_messages)) {}

		static PointerMessageTemplate fromYaml(const YAML::Node& node);
	};

	struct DiagnosticTemplate final {
		std::variant<MessageTemplate, ComponentTemplate, PointerMessageTemplate> content;

		static DiagnosticTemplate fromYaml(const YAML::Node& node);

		const Metadata& getMetadata() const {
			return std::visit([](auto& tpl) -> const Metadata& { return tpl.metadata; }, content);
		}
	};
}
