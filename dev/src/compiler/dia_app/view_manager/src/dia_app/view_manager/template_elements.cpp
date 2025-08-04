#include "template_elements.hpp"

namespace dia_app {
namespace message_template {

    // Parsing template elements.

    TemplatePtr parse(const YAML::Node& msg) {
        if (msg.IsScalar()) return std::make_shared<TextTElement>(msg.as<std::string>());
        if (msg.IsSequence()) return std::make_shared<ConcatTElement>(msg);
        if (msg["param"]) return std::make_shared<ParamTElement>(msg);
        if (msg["macro"]) return std::make_shared<MacroTElement>(msg);
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


    // ToDisplayVisitor methods' implementations

    ToDisplayVisitor::ToDisplayVisitor(
        ViewConstructor &vc,
        const InfoTemplate &info_template,
        const dia_file::InfoParams &info,
        const base::HashMap<std::string, dia_file::DisplayPtr> aux_params
    ) : vc(vc),
        info_template(info_template),
        info(info),
        aux_params(aux_params) {}
    
    void ToDisplayVisitor::visitTextTElement(const TextTElement &el) {
        res = std::make_shared<dia_file::TextDElement>(el.text);
    }
    void ToDisplayVisitor::visitConcatTElement(const ConcatTElement &el) {
        std::vector<dia_file::DisplayPtr> display_elems;

        // Collect results from all children of this element.
        for (const auto& elem: el.elems) {
            elem->accept(*this);
            display_elems.push_back(res);
        }
        // Gather children's results into a concat display element.
        res = std::make_shared<dia_file::ConcatDElement>(display_elems);
    }
    void ToDisplayVisitor::visitParamTElement(const ParamTElement &el) {
        assert(aux_params.count(el.param) || info.params.count(el.param));
        
        // Auxiliary parameters shadow the base ones.
        if (aux_params.contains(el.param)) {
            // Deep copy so independent transformations
            res = aux_params.at(el.param)->copy();
        } else {
            res = info.params.at(el.param)->copy();
        }
    }
    void ToDisplayVisitor::visitMacroTElement(const MacroTElement &el) {
        assert(info_template.macros.count(el.macro));
        // If this macro is already being evaluated, an infinite
        // recursion will occur.
        assert(!macro_stack.contains(el.macro));
        macro_stack.insert(el.macro);

        info_template.macros.at(el.macro)->accept(*this);

        macro_stack.erase(el.macro);
    }
    void ToDisplayVisitor::visitCaseOfTElement(const CaseOfTElement &el) {
        // Evaluate the pattern.
        el.pattern->accept(*this);
        // Evaluate the standard serialization string of
        // the given pattern.
        dia_file::ToTextVisitor v(vc);
        res->accept(v);
        auto pattern = v.builder;

        // Match the pattern against exact key matches.
        for (const auto &[key, val]: el.cases) {
            if (is_case_exact(key)) {
                if (key == pattern) {
                    // Found a match.
                    val->accept(*this);
                    return;
                }
            }
        }
        // No exact match found, match against class matches.
        // @TODO add more class matches here.

        // At last, whean all other matches failed,
        // match against the default match class.
        el.cases.at("[other]")->accept(*this);
    }
}
}