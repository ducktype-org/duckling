#pragma once
#include "dia_parser.hpp"
#include "template_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>

namespace dia_app {
	namespace message_template {

		struct TemplateElement;
		using TemplatePtr = std::shared_ptr<TemplateElement>;
		TemplatePtr parse(const YAML::Node& msg);

		struct TemplateElement {
			using DisplayPtr = dia_file::DisplayPtr;

			virtual ~TemplateElement() {}

			virtual DisplayPtr toDisplay(TemplateDataHandle handle) const = 0;
		};

		struct TextTElement: public TemplateElement {
			std::string text;

			TextTElement(const std::string& text): text(text) {}

			DisplayPtr toDisplay(TemplateDataHandle _) const override {
				return std::make_shared<dia_file::TextDElement>(text);
			}
		};

		struct ConcatTElement: public TemplateElement {
			std::vector<TemplatePtr> elems;

			ConcatTElement(const YAML::Node& elem_node) {
				assert(elem_node.IsSequence() && "concat element is not a sequence");
				for (const auto& el: elem_node) elems.push_back(parse(el));
			}

			DisplayPtr toDisplay(TemplateDataHandle handle) const override {
				std::vector<DisplayPtr> display_elems;
				for (const auto& elem: elems) display_elems.push_back(elem->toDisplay(handle));
				return std::make_shared<dia_file::ConcatDElement>(display_elems);
			}
		};

		struct ParamTElement: public TemplateElement {
			std::string param;

			ParamTElement(const YAML::Node& elem_node) {
				assert(elem_node["param"] && elem_node["param"].IsScalar());
				param = elem_node["param"].as<std::string>();
			}

			DisplayPtr toDisplay(TemplateDataHandle handle) const override {
				assert(handle.aux_params.count(param) || handle.param_data.params.count(param));
				
				// Auxiliary parameters shadow the base ones.
				if (handle.aux_params.contains(param)) {
					// Deep copy so independent transformations
					return handle.aux_params.at(param)->copy();
				} else {
					return handle.param_data.params.at(param)->copy();
				}
			}
		};

		struct MacroTElement: public TemplateElement {
			std::string macro;

			MacroTElement(const YAML::Node& elem_node) {
				assert(elem_node["macro"] && elem_node["macro"].IsScalar());
				macro = elem_node["macro"].as<std::string>();
			}

			DisplayPtr toDisplay(TemplateDataHandle handle) const override {
				assert(handle.template_data.macros.count(macro));
				// If this macro is already being evaluated, an infinite
				// recursion will occur.
				assert(!handle.macro_stack.contains(macro));
				handle.macro_stack.insert(macro);

				return handle.template_data.macros.at(macro)->toDisplay(handle);
			}
		};

		struct IncludeTElement: public TemplateElement {
			ShortMetadata include;
			TemplatePtr           on;

			IncludeTElement(const YAML::Node& elem_node) {
				assert(elem_node["include"]);
				include = elem_node["include"];
				assert(elem_node["on"]);
				on = parse(elem_node["on"]);
			}

			DisplayPtr toDisplay(TemplateDataHandle handle) const override {
				DisplayPtr res  = on->toDisplay(handle);
				InfoID info = InfoParamsHandle::add(include, handle.toDataHandle());
				res->assoc_infos.insert(info);
				return res;
			}
		};

		struct CaseOfTElement: public TemplateElement {
			TemplatePtr                        pattern;
			std::map<std::string, TemplatePtr> cases;

			CaseOfTElement(const YAML::Node& elem_node) {
				assert(elem_node["case"]);
				pattern = parse(elem_node["case"]);

				assert(elem_node["of"] && elem_node["of"].IsMap());
				for (const auto& it: elem_node["of"]) {
					const std::string key = it.first.as<std::string>();
					cases[key]            = parse(it.second);
				}
				assert(elem_node["of"]["[other]"] && "CaseOfTElement missing [other] case");
			}

			DisplayPtr toDisplay(TemplateDataHandle handle) const override {
				DisplayPtr pattern_display = pattern->toDisplay(handle);
				// Evaluate the standard serialization string of
				// the given match key.
				dia_file::ToTextVisitor v(handle.vc);
				pattern_display->accept(v);
				auto evalKey = v.builder;

				for (const auto& kv: cases) {
					const std::string& key = kv.first;
					const TemplatePtr&         val = kv.second;
					if (is_case_exact(key)) {
						if (key == evalKey)
							return val->toDisplay(handle);
					}
				}
				return cases.at("[other]")->toDisplay(handle);
			}
		};

		inline TemplatePtr parse(const YAML::Node& msg) {
			if (msg.IsScalar()) return std::make_shared<TextTElement>(msg.as<std::string>());
			if (msg.IsSequence()) return std::make_shared<ConcatTElement>(msg);
			if (msg["param"]) return std::make_shared<ParamTElement>(msg);
			if (msg["macro"]) return std::make_shared<MacroTElement>(msg);
			if (msg["include"]) return std::make_shared<IncludeTElement>(msg);
			return std::make_shared<CaseOfTElement>(msg);
		}

	}  // namespace message_template
}  // namespace dia_app
