#pragma once
#include "utils.hpp"
#include "template_parser.hpp"
#include "dia_parser.hpp"
#include "template_elements.hpp"

namespace dia_app {
namespace message_template {
    
    struct ToDisplayVisitor : public TemplateElementVisitor {
        ViewConstructor &vc;
        const TemplateData &template_data;
        const dia_file::InfoParams &info;

        // Auxiliary parameters for explore edges templates (shadow param_data).
        base::HashMap<std::string, dia_file::DisplayPtr> aux_params;

        // Macro evaluation stack for detecting infinite recursion.
        std::set<std::string> macro_stack;

        dia_file::DisplayPtr res;

        ToDisplayVisitor(
            ViewConstructor &vc,
            const TemplateData &template_data,
            const dia_file::InfoParams &info,
            const base::HashMap<std::string, dia_file::DisplayPtr> aux_params = {}
        ) : vc(vc),
            template_data(template_data),
            info(info),
            aux_params(aux_params) {}

        virtual void visitTextTElement(const TextTElement &el) {
            res = std::make_shared<dia_file::TextDElement>(el.text);
        }
        virtual void visitConcatTElement(const ConcatTElement &el) {
            std::vector<dia_file::DisplayPtr> display_elems;

            // Collect results from all children of this element.
            for (const auto& elem: el.elems) {
                elem->accept(*this);
                display_elems.push_back(res);
            }
            // Gather children's results into a concat display element.
            res = std::make_shared<dia_file::ConcatDElement>(display_elems);
        }
        virtual void visitParamTElement(const ParamTElement &el) {
            assert(aux_params.count(el.param) || info.params.count(el.param));
            
            // Auxiliary parameters shadow the base ones.
            if (aux_params.contains(el.param)) {
                // Deep copy so independent transformations
                res = aux_params.at(el.param)->copy();
            } else {
                res = info.params.at(el.param)->copy();
            }
        }
        virtual void visitMacroTElement(const MacroTElement &el) {
            assert(template_data.macros.count(el.macro));
            // If this macro is already being evaluated, an infinite
            // recursion will occur.
            assert(!macro_stack.contains(el.macro));
            macro_stack.insert(el.macro);

            template_data.macros.at(el.macro)->accept(*this);

            macro_stack.erase(el.macro);
        }
        virtual void visitIncludeTElement(const IncludeTElement &el) {
            // @TODO
            // DisplayPtr res  = on->toDisplay(handle);
            // InfoID info = InfoParamsHandle::add(include, handle.toDataHandle());
            // res->assoc_infos.insert(info);
            // return res;
        }
        virtual void visitCaseOfTElement(const CaseOfTElement &el) {
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
    };

    struct ExploreEdge {
        dia_file::DisplayPtr description;
        InfoID handle;

        std::string getDescription(ViewConstructor &vc) {
            dia_file::ToTextVisitor v(vc);
            description->accept(v);
            return v.builder;
        }

        ExploreEdge(dia_file::DisplayPtr description, InfoID handle) :
            description(description), handle(handle) {}
    };

    struct DisplayPointerMessage {
        u32 priority;
        std::string type;
        dia_file::DisplayPtr message;

        DisplayPointerMessage() {}
        DisplayPointerMessage(const PointerMessage &msg, ToDisplayVisitor &v) :
            priority(msg.priority), type(msg.type) {
            // Evaluate the attached message.
            msg.message->accept(v);
            message = v.res;
            ASSUME(message, "no pointer message after template application");
        }
    };

    struct Info {
        using CodeData = dia_file::CodeData;

        Metadata metadata;
        dia_file::DisplayPtr header_message;
        base::Optional<CodeData> code;
        base::HashMap<std::string, DisplayPointerMessage> pointer_messages;
        dia_file::DisplayPtr description;
        std::vector<ExploreEdge> explore_edges;

        Info(ViewConstructor &vc,
            const TemplateData &template_data,
            const dia_file::InfoParams &info
        ) :
            metadata(template_data.metadata),
            code(info.code) {

            ToDisplayVisitor v(vc, template_data, info);

            // Evaluate the header message template.
            template_data.header_message->accept(v);
            header_message = v.res;
            ASSUME(header_message, "no header message after template application");

            // Evaluate pointer message templates.
            for (auto &[key, val] : template_data.pointer_messages) {
                pointer_messages.put(key, DisplayPointerMessage(val, v));
            }

            // Verify code metadata against the template.
            verify_code();

            // Description can be nullptr (optional).
            if (template_data.description) {
                template_data.description->accept(v);
                description = v.res;
            }

            // Evaluate explore edge templates.
            for (auto &edge : info.explore_edges) {
                auto edge_template = template_data.explore_edges.at(edge.name);
                // Remember to include auxiliary parameters included in the explore edge.
                v.aux_params = edge.params;
                edge_template->accept(v);
                // In explore edges only plain text is displayed.
                dia_file::ToTextVisitor v_text(vc);
				v.res->accept(v_text);
                auto description = v_text.builder;

                explore_edges.emplace_back(description, edge.handle);
            }
        }

        void verify_code() const {
            using namespace dia_file;

            struct VerifyCodeVisitor : public dia_file::DisplayElementVisitor {
                bool is_ok = true;
                const Info *info;

                VerifyCodeVisitor(const Info *info) : info(info) {}

                virtual void visitTextDElement(const TextDElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitCodeDElement(const CodeDElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitConcatDElement(const ConcatDElement &el) {
                    verify_groups(el.groups);
                    for (auto &child : el.elems) {
                        child->accept(*this);
                    }
                }
                virtual void visitStartLineDElement(const StartLineDElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitInteractDElement(const InteractDElement &el) {
                    verify_groups(el.groups);
                    el.content->accept(*this);
                    el.alt_content->accept(*this);
                }
                virtual void visitEntityDElement(const EntityDElement &el) {
                    verify_groups(el.groups);
                    el.content->accept(*this);
                }
                virtual void visitLazyDElement(const LazyDElement &el) {
                    verify_groups(el.groups);
                    // TODO: remember about verification upon fetching.
                }

                void verify_groups(const std::set<std::string> &groups) {
                    for (auto &g : groups) {
                        if (!info->pointer_messages.contains(g)) {
                            is_ok = false;
                            break;
                        }
                    }
                }
            };
            
            if_opt_some(code, code_v) {
                VerifyCodeVisitor v(this);
                code_v.content->accept(v);
                ASSERT(v.is_ok, "some component inside info code refers to a non-existent group");
            }
        }
    };

    /*
        Apply a message template identified and parametrized by `data`
        and store the result in `result`.
        
        Return true upon success.
    */
    inline Info apply(
        ViewConstructor &vc,
        const dia_file::InfoParams &info
    ) {
        try {
            TemplateData template_data(info);
            return Info(vc, template_data, info);
        } catch (TemplateFileNotFoundException e) {
            ASSUME(false, "message template file was not found on disk");
        }
    }

} // namespace message_template
} // namespace dia_app