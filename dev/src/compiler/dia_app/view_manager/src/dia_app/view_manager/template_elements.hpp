#pragma once
#include "dia_parser.hpp"
#include "template_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>
#include <base/visitor.hpp>

namespace dia_app {
	namespace message_template {

		struct TemplateElement;
		using TemplatePtr = std::shared_ptr<TemplateElement>;
		TemplatePtr parse(const YAML::Node& msg);

		struct TextTElement;
		struct ConcatTElement;
		struct ParamTElement;
		struct MacroTElement;
		struct IncludeTElement;
		struct CaseOfTElement;

		MAKE_VISITOR(TemplateElement,
			TextTElement,
			ConcatTElement,
			ParamTElement,
			MacroTElement,
			IncludeTElement,
			CaseOfTElement
		);

		struct TemplateElement {
			using DisplayPtr = dia_file::DisplayPtr;

			virtual ~TemplateElement() {}

			virtual void accept(TemplateElementVisitor &visitor) = 0;
		};

		struct TextTElement: public TemplateElement {
			std::string text;

			TextTElement(const std::string& text);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitTextTElement(*this);
			}
		};

		struct ConcatTElement: public TemplateElement {
			std::vector<TemplatePtr> elems;

			ConcatTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitConcatTElement(*this);
			}
		};

		struct ParamTElement: public TemplateElement {
			std::string param;

			ParamTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitParamTElement(*this);
			}
		};

		struct MacroTElement: public TemplateElement {
			std::string macro;

			MacroTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitMacroTElement(*this);
			}
		};

		struct IncludeTElement: public TemplateElement {
			ShortMetadata include;
			TemplatePtr           on;

			IncludeTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitIncludeTElement(*this);
			}
		};

		struct CaseOfTElement: public TemplateElement {
			TemplatePtr                        pattern;
			base::Map<std::string, TemplatePtr> cases;

			CaseOfTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor &visitor) {
				visitor.visitCaseOfTElement(*this);
			}
		};

	}  // namespace message_template
}  // namespace dia_app
