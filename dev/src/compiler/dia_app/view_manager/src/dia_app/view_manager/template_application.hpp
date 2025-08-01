#pragma once
#include "utils.hpp"
#include "template_parser.hpp"
#include "dia_parser.hpp"
#include "template_elements.hpp"

namespace dia_app {
namespace message_template {

    // Apply the message template to message.
    inline dia_file::DisplayPtr apply(TemplatePtr message, TemplateDataHandle handle) {
        if (!message) return nullptr;
        
        // Evaluate the template.
        return message->toDisplay(handle);
    }

    struct DisplayPointerMessage {
        u32 priority;
        std::string type;
        dia_file::DisplayPtr message;

        DisplayPointerMessage() {}
        DisplayPointerMessage(const PointerMessage &msg, TemplateDataHandle handle) :
            priority(msg.priority), type(msg.type) {
            message = apply(msg.message, handle);
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

        Info(TemplateDataHandle handle) :
            metadata(handle.template_data.metadata),
            code(handle.param_data.code) {

            header_message = apply(handle.template_data.header_message, handle);
            ASSUME(header_message, "no header message after template application");

            for (auto &[key, val] : handle.template_data.pointer_messages) {
                pointer_messages.put(key, DisplayPointerMessage(val, handle));
            }

            // Verify code metadata against the template.
            verify_code();

            // Description can be nullptr.
            description = apply(handle.template_data.description, handle);

            for (auto &edge : handle.param_data.explore_edges) {
                auto edge_template = handle.template_data.explore_edges.at(edge.name);
                // Remember to include auxiliary parameters included in the explore edge.
                auto description_ptr = apply(edge_template, handle.with_aux_params(edge.params));
                // In explore edges only plain text is displayed.
                dia_file::ToTextVisitor v(handle.vc);
				description_ptr->accept(v);
                auto description = v.builder;

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
        const dia_file::InfoParams &param_data
    ) {
        try {
            TemplateData template_data(param_data);
            return Info(TemplateDataHandle(vc, template_data, param_data));
        } catch (TemplateFileNotFoundException e) {
            ASSUME(false, "message template file was not found on disk");
        }
    }

} // namespace message_template
} // namespace dia_app