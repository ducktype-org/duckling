#pragma once
#include "utils.hpp"
#include "template_parser.hpp"
#include "dia_parser.hpp"
#include "template_elements.hpp"

namespace dia_app {
namespace message_template {

    // Apply the message template to message.
    inline dia_file::Ptr apply(Ptr message, TemplateDataHandle handle) {
        if (!message) return nullptr;
        
        // Evaluate the template.
        return message->toDisplay(handle);
    }

    struct DisplayPointerMessage {
        u32 priority;
        std::string type;
        dia_file::Ptr message;

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
        dia_file::Ptr header_message;
        base::Optional<CodeData> code;
        base::HashMap<std::string, DisplayPointerMessage> pointer_messages;
        dia_file::Ptr description;
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
                auto description = description_ptr->toText(handle.toDataHandle());

                explore_edges.emplace_back(description, edge.handle);
            }
        }

        void verify_code() const {
            struct VerifyCodeVisitor : public dia_file::DisplayElementVisitor {
                bool is_ok = true;
                const Info *info;

                VerifyCodeVisitor(const Info *info) : info(info) {}

                virtual void visitTextElement(const dia_file::TextElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitCodeElement(const dia_file::CodeElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitConcatElement(const dia_file::ConcatElement &el) {
                    verify_groups(el.groups);
                    for (auto &child : el.elems) {
                        child->accept(*this);
                    }
                }
                virtual void visitStartLineElement(const dia_file::StartLineElement &el) {
                    verify_groups(el.groups);
                }
                virtual void visitInteractElement(const dia_file::InteractElement &el) {
                    verify_groups(el.groups);
                    el.content->accept(*this);
                    el.alt_content->accept(*this);
                }
                virtual void visitEntityElement(const dia_file::EntityElement &el) {
                    verify_groups(el.groups);
                    el.content->accept(*this);
                }
                virtual void visitLazyElement(const dia_file::LazyElement &el) {
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

    enum class Error {
        TemplateFileNotFound
    };

    /*
        Apply a message template identified and parametrized by `data`
        and store the result in `result`.
        
        Return true upon success.
    */
    inline std::expected<Info, Error> apply(
        const dia_file::ParamData &param_data,
        DataHandle data_handle
    ) {
        try {
            TemplateData template_data(param_data);
            return Info(TemplateDataHandle(template_data, param_data, data_handle));
        } catch (TemplateFileNotFoundException e) {
            return std::unexpected(Error::TemplateFileNotFound);
        }
    }

} // namespace message_template
} // namespace dia_app