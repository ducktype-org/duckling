#pragma once
#include "dia_parser.hpp"
#include "template_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>

#include <base/visitor.hpp>

namespace dia_app {
	namespace message_template {

		// Forward declarations of template elements and a template visitor.

		struct TemplateElement;
		using TemplatePtr = std::shared_ptr<TemplateElement>;

		struct TextTElement;
		struct ConcatTElement;
		struct ParamTElement;
		struct MacroTElement;
		struct CaseOfTElement;

		MAKE_VISITOR(TemplateElement,
			TextTElement,
			ConcatTElement,
			ParamTElement,
			MacroTElement,
			CaseOfTElement
		);

		/**
		 * @brief Parse the YAML representation of a template element.
		 *
		 * @param msg The YAML representation
		 * @return TemplatePtr
		 */
		TemplatePtr parse(const YAML::Node& msg);

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
		struct TemplateElement {
			virtual ~TemplateElement() {}

			virtual void accept(TemplateElementVisitor& visitor) = 0;
		};

		/**
		 * @brief A template element representing simple text.
		 *
		 */
		struct TextTElement: public TemplateElement {
			std::string text;

			TextTElement(const std::string& text);

			virtual void accept(TemplateElementVisitor& visitor) {
				visitor.visitTextTElement(*this);
			}
		};

		/**
		 * @brief A template element representing a concatenation
		 * of other template elements.
		 *
		 */
		struct ConcatTElement: public TemplateElement {
			std::vector<TemplatePtr> elems;

			ConcatTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor& visitor) {
				visitor.visitConcatTElement(*this);
			}
		};

		/**
		 * @brief A template element representing a parameter passed down
		 * by the compiler.
		 *
		 */
		struct ParamTElement: public TemplateElement {
			std::string param;

			ParamTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor& visitor) {
				visitor.visitParamTElement(*this);
			}
		};

		/**
		 * @brief A template element representing an expansion of
		 * a template macro.
		 *
		 */
		struct MacroTElement: public TemplateElement {
			std::string macro;

			MacroTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor& visitor) {
				visitor.visitMacroTElement(*this);
			}
		};

		/**
		 * @brief A template element representing a logical branching
		 * of template evaluation based on the provided pattern.
		 *
		 */
		struct CaseOfTElement: public TemplateElement {
			TemplatePtr                         pattern;
			base::Map<std::string, TemplatePtr> cases;

			CaseOfTElement(const YAML::Node& elem_node);

			virtual void accept(TemplateElementVisitor& visitor) {
				visitor.visitCaseOfTElement(*this);
			}
		};

		/**
		 * @brief A template element visitor for conversion to display elements.
		 *
		 * Note: the visitor can be reused with different auxiliary parameters.
		 */
		struct ToDisplayVisitor: public TemplateElementVisitor {
		private:
			// A view constructor reference for potential fetching of resources
			// upon evaluating the pattern of `CaseOfTElement`.
			ViewConstructor& vc;
			// Info template including a list of macros.
			const InfoTemplate& info_template;
			// Info parameters passed down by the compiler.
			const dia_file::InfoParams& info;

			// Macro evaluation stack for detecting infinite recursion.
			std::set<std::string> macro_stack;

		public:
			// Auxiliary parameters for explore edges templates
			// (they shadow param_data).
			base::HashMap<std::string, dia_file::DisplayPtr> aux_params;

			// The result of a visitor run.
			dia_file::DisplayPtr res;

			ToDisplayVisitor(
				ViewConstructor&                                       vc,
				const InfoTemplate&                                    info_template,
				const dia_file::InfoParams&                            info,
				const base::HashMap<std::string, dia_file::DisplayPtr> aux_params = {}
			);

			virtual void visitTextTElement(const TextTElement& el);
			virtual void visitConcatTElement(const ConcatTElement& el);
			virtual void visitParamTElement(const ParamTElement& el);
			virtual void visitMacroTElement(const MacroTElement& el);
			virtual void visitCaseOfTElement(const CaseOfTElement& el);
		};
	}  // namespace message_template
}  // namespace dia_app
