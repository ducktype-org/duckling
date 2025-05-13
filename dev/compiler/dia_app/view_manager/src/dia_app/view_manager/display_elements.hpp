#pragma once
#include "utils.hpp"
#include "components.hpp"

namespace dia_app {
namespace dia_file {

    // Element which can be displayed as a part of text.
    struct DisplayElement;
    using Ptr = std::shared_ptr<DisplayElement>;

    Ptr parse(const json &msg);

    struct DisplayElement {
        std::vector<InfoHandle> assoc_infos;

        DisplayElement(const std::vector<InfoHandle> assoc_infos) : assoc_infos(assoc_infos) {}
        virtual ~DisplayElement();
        
        // Make a deep copy of this element.
        virtual Ptr copy() const = 0;
        // Associate this element with info.
        void add_assoc_info(InfoHandle info) {
            if (std::find(assoc_infos.begin(), assoc_infos.end(), info) == assoc_infos.end()) {
                assoc_infos.push_back(info);
            }
        }
        // Get all associated infos on this particular element.
        // Note: this does *not* get all associated infos from
        //       the entire subtree.
        virtual std::vector<InfoHandle> get_assoc_infos(DataHandle dh) const {
            return assoc_infos;
        }
        // Check whether this particular element has
        // any associated infos. May fetch some entities.
        bool has_assoc_infos(DataHandle dh) const {
            return get_assoc_infos(dh).size() > 0;
        }
        // Evaluate the pure text content of this element.
        // Only evaluates the default content (alt_content is ignored).
        // Used e.g. for "case of" matching in message templates.
        virtual std::string to_text(DataHandle dh) const = 0;

        // Evaluate this element (not its subtree).
        // If called on a handle, fetches the resource.
        virtual Ptr evaluated(Ptr me, DataHandle _) const {
            return me;
        }

        // Serialize this element (DEBUG ONLY).
        virtual json show(DataHandle dh) const = 0;

        virtual std::shared_ptr<Component> toComponent(CreationContext &creation_context) const;
    };

    struct TextElement : public DisplayElement {
        std::string content;
        std::vector<std::string> groups;

        TextElement(const json &elem_json);
        TextElement(const std::string &content);
        TextElement(const TextElement &other);

        Ptr copy() const;
        std::string to_text(DataHandle _) const;

        json show(DataHandle _) const;

        virtual std::shared_ptr<Component> toComponent(CreationContext &creation_context) const override {
            return static_pointer_cast<Component>(make_shared<TextComponent>(content));
        }
    };

    struct ConcatElement : public DisplayElement {
        std::vector<Ptr> elems;
        std::vector<std::string> groups;

        ConcatElement(const json &elem_json);
        ConcatElement(const std::vector<Ptr> &elems, const std::vector<InfoHandle> &assoc_infos);

        Ptr copy() const;
        std::string to_text(DataHandle dh) const;

        json show(DataHandle dh) const;

        virtual std::shared_ptr<Component> toComponent(CreationContext &creation_context) const override {
            vector<shared_ptr<Component>> sons;
            transform(this->elems.begin(), this->elems.end(), sons.begin(),
                [&creation_context](const Ptr &son) {
                return son->toComponent(creation_context);
            });
            return static_pointer_cast<Component>(make_shared<ConcatComponent>(sons));
        }
    };

    struct StartLineElement : public DisplayElement {
        // Line number.
        std::optional<uint> number;

        StartLineElement(const json &elem_json);
        StartLineElement(const StartLineElement &other);

        Ptr copy() const;
        std::string to_text(DataHandle dh) const;

        json show(DataHandle dh) const;

        virtual std::shared_ptr<Component> toComponent(CreationContext &creation_context) const override {
            return static_pointer_cast<Component>(make_shared<StartLineComponent>(this->number));
        }
    };

    struct InteractElement : public DisplayElement {
        Ptr content;
        Ptr alt_content;

        InteractElement(const json &elem_json);
        InteractElement(Ptr content, Ptr alt_content, const std::vector<InfoHandle> &assoc_infos);

        Ptr copy() const;
        std::string to_text(DataHandle dh) const;

        json show(DataHandle dh) const;

        virtual std::shared_ptr<Component> toComponent(CreationContext &creation_context) const override {
            auto primary = this->content->toComponent(creation_context);
            auto alternative = this->alt_content->toComponent(creation_context);
            auto result = make_shared<InteractiveComponent>(getNewId(), primary, alternative);
            creation_context.id_to_interactive_component->emplace(result->getId(), std::weak_ptr<InteractiveComponent>(result));
            return static_pointer_cast<Component>(result);
        }
    };

    struct LazyElement : public DisplayElement {
        ResourceHandle handle;

        LazyElement(const json &elem_json);
        LazyElement(const LazyElement &other);

        Ptr copy() const;
        std::string to_text(DataHandle dh) const;

        Ptr evaluated(Ptr _, DataHandle dh) const;

        json show(DataHandle dh) const;
    };

    struct EntityElement : public DisplayElement {
        std::string refers_to;
        Ptr content;

        EntityElement(const json &elem_json);
        EntityElement(const std::string &refers_to, Ptr content, const std::vector<InfoHandle> &assoc_infos);

        Ptr copy() const;
        std::vector<InfoHandle> get_assoc_infos(DataHandle dh) const;

        std::string to_text(DataHandle dh) const;
    
        json show(DataHandle dh) const;
    };

    struct CodeElement : public DisplayElement {
        Ptr content;

        CodeElement(const json &elem_json);
        CodeElement(Ptr content, const std::vector<InfoHandle> &assoc_infos);

        Ptr copy() const;

        std::string to_text(DataHandle dh) const;

        json show(DataHandle dh) const;
    };

} // namespace dia_file
} // namespace dia_app