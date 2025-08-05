#include "template_application.hpp"

namespace dia_app {
namespace message_template {

    ExploreEdge::ExploreEdge(dia_file::DisplayPtr description, InfoID info_id)
        : description(description), info_id(info_id) {}
        
    std::string ExploreEdge::getDescription(ViewConstructor &vc) {
        dia_file::ToTextVisitor v(vc);
        description->accept(v);
        return v.builder;
    }


    DisplayPointerMessage::DisplayPointerMessage() {}
    DisplayPointerMessage::DisplayPointerMessage(const PointerMessage &msg, ToDisplayVisitor &v)
        : priority(msg.priority), type(msg.type) {
        // Evaluate the attached message.
        msg.message->accept(v);
        message = v.res;
        ASSUME(message, "no pointer message after template application");
    }


    Info::Info(
        ViewConstructor &vc,
        const InfoTemplate &info_template,
        const dia_file::InfoParams &info
    ) :
        metadata(info_template.metadata),
        code(info.code) {

        ToDisplayVisitor v(vc, info_template, info);

        // Evaluate the header message template.
        info_template.header_message->accept(v);
        header_message = v.res;
        ASSUME(header_message, "no header message after template application");

        // Evaluate pointer message templates.
        for (auto &[key, val] : info_template.pointer_messages) {
            pointer_messages.put(key, DisplayPointerMessage(val, v));
        }

        // Verify code metadata against the template.
        verify_code();

        // Description can be nullptr (optional).
        if (info_template.description) {
            info_template.description->accept(v);
            description = v.res;
        }

        // Evaluate explore edge templates.
        for (auto &edge : info.explore_edges) {
            auto edge_template = info_template.explore_edges.at(edge.name);
            // Remember to include auxiliary parameters included in the explore edge.
            v.aux_params = edge.params;
            edge_template->accept(v);
            // In explore edges only plain text is displayed.
            dia_file::ToTextVisitor v_text(vc);
            v.res->accept(v_text);
            auto description = v_text.builder;

            explore_edges.emplace_back(description, edge.info_id);
        }
    }

    void Info::verify_code() const {
        using namespace dia_file;

        struct VerifyCodeVisitor : public DisplayElementVisitor {
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


    Info apply(ViewConstructor &vc, const dia_file::InfoParams &info) {
        try {
            InfoTemplate info_template(info);
            return Info(vc, info_template, info);
        } catch (TemplateFileNotFoundException e) {
            ASSUME(false, "message template file was not found on disk");
        }
    }
}
}