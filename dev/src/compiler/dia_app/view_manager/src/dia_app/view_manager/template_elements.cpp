#include "template_elements.hpp"

namespace dia_app {
namespace message_template {

    TemplatePtr parse(const YAML::Node& msg) {
        if (msg.IsScalar()) return std::make_shared<TextTElement>(msg.as<std::string>());
        if (msg.IsSequence()) return std::make_shared<ConcatTElement>(msg);
        if (msg["param"]) return std::make_shared<ParamTElement>(msg);
        if (msg["macro"]) return std::make_shared<MacroTElement>(msg);
        if (msg["include"]) return std::make_shared<IncludeTElement>(msg);
        return std::make_shared<CaseOfTElement>(msg);
    }

    TextTElement::TextTElement(const std::string &text) : text(text) {}

    ConcatTElement::ConcatTElement(const YAML::Node &elem_node) {
        assert(elem_node.IsSequence() && "concat element is not a sequence");
        for (const auto& el: elem_node) elems.push_back(parse(el));
    }

    ParamTElement::ParamTElement(const YAML::Node& elem_node) {
        assert(elem_node["param"] && elem_node["param"].IsScalar());
        param = elem_node["param"].as<std::string>();
    }

    MacroTElement::MacroTElement(const YAML::Node& elem_node) {
        assert(elem_node["macro"] && elem_node["macro"].IsScalar());
        macro = elem_node["macro"].as<std::string>();
    }

    IncludeTElement::IncludeTElement(const YAML::Node& elem_node) {
        assert(elem_node["include"]);
        include = elem_node["include"];
        assert(elem_node["on"]);
        on = parse(elem_node["on"]);
    }

    CaseOfTElement::CaseOfTElement(const YAML::Node& elem_node) {
        assert(elem_node["case"]);
        pattern = parse(elem_node["case"]);

        assert(elem_node["of"] && elem_node["of"].IsMap());
        for (const auto& it: elem_node["of"]) {
            const std::string key = it.first.as<std::string>();
            cases.put(key, parse(it.second));
        }
        assert(elem_node["of"]["[other]"] && "CaseOfTElement missing [other] case");
    }
}
}