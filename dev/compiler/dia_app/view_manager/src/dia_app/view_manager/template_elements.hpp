#pragma once
#include <yaml-cpp/yaml.h>
#include "utils.hpp"
#include "dia_parser.hpp"
#include "template_parser.hpp"

namespace dia_app {
namespace message_template {
    
    struct TemplateElement;
    using Ptr = std::shared_ptr<TemplateElement>;
    Ptr parse(const YAML::Node &msg);
    
    struct TemplateElement {
        using DisplayPtr = dia_file::Ptr;

        virtual ~TemplateElement() {}
        virtual DisplayPtr toDisplay(TemplateDataHandle handle) const = 0;
    };

    struct TextElement : public TemplateElement {
        std::string text;

        TextElement(const std::string &text) : text(text) {}

        DisplayPtr toDisplay(TemplateDataHandle _) const override {
            return std::make_shared<dia_file::TextElement>(text);
        }
    };

    struct ConcatElement : public TemplateElement {
        std::vector<Ptr> elems;
        
        ConcatElement(const YAML::Node &elem_node) {
            assert(elem_node.IsSequence() && "concat element is not a sequence");
            for (const auto &el : elem_node) {
                elems.push_back(parse(el));
            }
        }

        DisplayPtr toDisplay(TemplateDataHandle handle) const override {
            std::vector<DisplayPtr> display_elems;
            for (const auto &elem : elems) {
                display_elems.push_back(elem->toDisplay(handle));
            }
            return std::make_shared<dia_file::ConcatElement>(display_elems);
        }
    };

    struct ParamElement : public TemplateElement {
        std::string param;

        ParamElement(const YAML::Node &elem_node) {
            assert(elem_node["param"] && elem_node["param"].IsScalar());
            param = elem_node["param"].as<std::string>();
        }

        DisplayPtr toDisplay(TemplateDataHandle handle) const override {
            assert(handle.param_data.params.count(param));
            // Deep copy so independent transformations
            return handle.param_data.params.at(param)->copy();
        }
    };

    struct MacroElement : public TemplateElement {
        std::string macro;

        MacroElement(const YAML::Node &elem_node) {
            assert(elem_node["macro"] && elem_node["macro"].IsScalar());
            macro = elem_node["macro"].as<std::string>();
        }

        DisplayPtr toDisplay(TemplateDataHandle handle) const override {
            assert(handle.template_data.macros.count(macro));
            return handle.template_data.macros.at(macro)->toDisplay(handle);
        }
    };

    struct IncludeElement : public TemplateElement {
        ShortMetadata include;
        Ptr on;

        IncludeElement(const YAML::Node &elem_node) {
            assert(elem_node["include"]);
            include = elem_node["include"];
            assert(elem_node["on"]);
            on = parse(elem_node["on"]);
        }

        DisplayPtr toDisplay(TemplateDataHandle handle) const override {
            DisplayPtr res = on->toDisplay(handle);
            InfoHandle info = InfoParamsHandle::add(include, handle.toDataHandle());
            res->assoc_infos.insert(info);
            return res;
        }
    };

    struct CaseOfElement : public TemplateElement {
        Ptr pattern;
        std::map<std::string, Ptr> cases;

        CaseOfElement(const YAML::Node &elem_node) {
            assert(elem_node["case"]);
            pattern = parse(elem_node["case"]);

            assert(elem_node["of"] && elem_node["of"].IsMap());
            for (const auto &it : elem_node["of"]) {
                const std::string key = it.first.as<std::string>();
                cases[key] = parse(it.second);
            }
            assert(elem_node["of"]["[other]"] && "CaseOfElement missing [other] case");
        }

        DisplayPtr toDisplay(TemplateDataHandle handle) const override {
            DisplayPtr pattern_display = pattern->toDisplay(handle);
            for (const auto &kv : cases) {
                const std::string &key = kv.first;
                const Ptr &val = kv.second;
                if (is_case_exact(key)) {
                    if (key == pattern_display->toText(handle.toDataHandle())) {
                        return val->toDisplay(handle);
                    }
                }
            }
            return cases.at("[other]")->toDisplay(handle);
        }
    };

    inline Ptr parse(const YAML::Node &msg) {
        if (msg.IsScalar()) {
            return std::make_shared<TextElement>(msg.as<std::string>());
        }
        if (msg.IsSequence()) {
            return std::make_shared<ConcatElement>(msg);
        }
        if (msg["param"]) {
            return std::make_shared<ParamElement>(msg);
        }
        if (msg["macro"]) {
            return std::make_shared<MacroElement>(msg);
        }
        if (msg["include"]) {
            return std::make_shared<IncludeElement>(msg);
        }
        return std::make_shared<CaseOfElement>(msg);
    }

} // namespace message_template
} // namespace dia_app
