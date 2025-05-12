#pragma once
#include "utils.hpp"
#include "dia_parser.hpp"
#include "template_parser.hpp"

namespace dia_app {
namespace message_template {
    
    struct TemplateElement;
    using Ptr = std::shared_ptr<TemplateElement>;
    Ptr parse(const json &msg);
    
    struct TemplateElement {
        using DisplayPtr = dia_file::Ptr;

        virtual ~TemplateElement() {}
        virtual DisplayPtr to_display(TemplateDataHandle handle) const = 0;
    };

    struct TextElement : public TemplateElement {
        std::string text;

        TextElement(const std::string &text) : text(text) {}

        DisplayPtr to_display(TemplateDataHandle _) const {
            return std::make_shared<dia_file::TextElement>(text);
        }
    };
    struct ConcatElement : public TemplateElement {
        std::vector<Ptr> elems;
        
        ConcatElement(const json &elem_json) {
            ASSUME(elem_json.is_array(), "concat element is not an array");
            for (const json &el : elem_json) {
                elems.push_back(parse(el));
            }
        }

        DisplayPtr to_display(TemplateDataHandle handle) const {
            std::vector<DisplayPtr> display_elems;
            for (auto &elem : elems) {
                display_elems.push_back(elem->to_display(handle));
            }
            return std::make_shared<dia_file::ConcatElement>(display_elems, std::vector<uint>());
        }
    };
    struct ParamElement : public TemplateElement {
        std::string param;

        ParamElement(const json &elem_json) {
            ASSUME_HAS_STR_ASSIGN(elem_json, param);
        }

        DisplayPtr to_display(TemplateDataHandle handle) const {
            ASSUME_HAS(handle.param_data.params, param);
            // A deep copy is performed, so that later transformations
            // on multiple occurences of the same parameter happen independently.
            return handle.param_data.params.at(param)->copy();
        }
    };
    struct MacroElement : public TemplateElement {
        std::string macro;

        MacroElement(const json &elem_json) {
            ASSUME_HAS_STR_ASSIGN(elem_json, macro);
        }

        DisplayPtr to_display(TemplateDataHandle handle) const {
            ASSUME_HAS(handle.template_data.macros, macro);
            return handle.template_data.macros.at(macro)->to_display(handle);
        }
    };
    struct IncludeElement : public TemplateElement {
        ShortMetadata include;
        Ptr on;

        IncludeElement(const json &elem_json) {
            ASSUME_HAS(elem_json, "include");
            include = elem_json["include"];
            ASSUME_HAS(elem_json, "on");
            on = parse(elem_json["on"]);
        }

        DisplayPtr to_display(TemplateDataHandle handle) const {
            // Generate the display element.
            DisplayPtr res = on->to_display(handle);

            // Create (or access if present) a new info handle
            // with include metadata.
            InfoHandle info = InfoParamsHandle::add(include, handle.to_data_handle());
            // Assign the new info handle to the display element.
            res->add_assoc_info(info);

            return res;
        }
    };
    struct CaseOfElement : public TemplateElement {
        Ptr pattern;
        std::map<std::string, Ptr> cases;

        CaseOfElement(const json &elem_json) {
            ASSUME_HAS(elem_json, "case");
            pattern = parse(elem_json["case"]);

            ASSUME_HAS(elem_json, "of");
            for (auto &[key, val] : elem_json["of"].items()) {
                cases[key] = parse(val);
            }

            ASSUME_HAS(elem_json["of"], "[other]");
        }

        DisplayPtr to_display(TemplateDataHandle handle) const {
            // Generate the pattern display content.
            // Note: this display content will *not* be displayed,
            //       only pattern-matched, so it might e.g. introduce some
            //       redundant (unreachable) infos, but we are OK with that cost.
            DisplayPtr pattern_display = pattern->to_display(handle);
            // Match on cases and generate the matching display content.
            // Exact matches first.
            for (auto &[key, val] : cases) {
                if (is_case_exact(key)) {
                    if (key == pattern_display->to_text(handle.to_data_handle())) {
                        return val->to_display(handle);
                    }
                }
            }
            // Note: some other [class] matches can be introduced and matched here.
            // [other] last.
            return cases.at("[other]")->to_display(handle);
        }
    };

    /* Order of message type evaluation:
        text -> concat -> parameter -> macro -> case of */
    inline Ptr parse(const json &msg) {
        if (msg.is_string()) {
            return std::make_shared<TextElement>(msg);
        }
        if (msg.is_array()) {
            return std::make_shared<ConcatElement>(msg);
        }
        if (msg.contains("param")) {
            return std::make_shared<ParamElement>(msg);
        }
        if (msg.contains("macro")) {
            return std::make_shared<MacroElement>(msg);
        }
        if (msg.contains("include")) {
            return std::make_shared<IncludeElement>(msg);
        }
        return std::make_shared<CaseOfElement>(msg);
    }
} // namespace message_template
} // namespace dia_app