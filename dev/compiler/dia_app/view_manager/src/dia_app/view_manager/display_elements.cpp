#include "display_elements.hpp"

namespace dia_app {
namespace dia_file {
    using view_manager::Component,
          view_manager::InteractiveComponent,
          view_manager::ConcatComponent,
          view_manager::TextComponent,
          view_manager::CodeComponent,
          view_manager::StartLineComponent,
          view_manager::CreationContext;

    Ptr fetch_resource(ResourceHandle, DataHandle data_handle);
    void fetch_entity(EntityHandle entity_handle, DataHandle data_handle);
    std::vector<InfoHandle> scan_entity_metadata(const json &entity, DataHandle handle);

    // ---------------- DisplayElement ---------------- //

    DisplayElement::~DisplayElement() {}
    std::shared_ptr<Component> DisplayElement::toComponent(CreationContext&) {
        return {};
    }

    // ---------------- TextElement ---------------- //

    TextElement::TextElement(const json &elem_json) : DisplayElement({}) {
        if (elem_json.is_string()) {
            // Plain text, no metadata.
            content = elem_json;
            return;
        }
        ASSUME_HAS(elem_json, "type");
        ASSUME_VAL(elem_json, "type", "text");
        ASSUME_HAS_STR_ASSIGN(elem_json, content);

        if (elem_json.contains("groups")) {
            ASSUME_ARR(elem_json, "groups");
            groups = elem_json["groups"];
        }
    }
    TextElement::TextElement(const std::string &content) : DisplayElement({}), content(content) {}
    TextElement::TextElement(const TextElement &other) :
        DisplayElement(other.assoc_infos),
        content(other.content),
        groups(other.groups) {}

    Ptr TextElement::copy() const {
        return std::make_shared<TextElement>(*this);
    }

    std::string TextElement::to_text(DataHandle _) const {
        return content;
    }

    json TextElement::show(DataHandle _) const {
        json res;
        res["type"] = "text";
        res["content"] = content;
        res["groups"] = groups;
        return res;
    }

    std::shared_ptr<Component> TextElement::toComponent(CreationContext &creation_context) {
        if (!this->generated_component) {
            this->generated_component = static_pointer_cast<Component>(make_shared<TextComponent>(content));
        }
        return this->generated_component;
    }


    // ---------------- ConcatElement ---------------- //

    ConcatElement::ConcatElement(const json &elem_json) : DisplayElement({}) {
        if (elem_json.is_array()) {
            // A simple array of elements.
            for (auto &el : elem_json) {
                elems.push_back(parse(el));
            }
        } else {
            // An element grouping with possible pointer message groups.
            ASSUME_HAS(elem_json, "type");
            ASSUME_VAL(elem_json, "type", "grouping");

            if (elem_json.contains("groups")) {
                ASSUME_ARR(elem_json, "groups");
                groups = elem_json["groups"];
            }

            // We only create thin concat elements from element groupings.
            ASSUME_HAS(elem_json, "content");
            elems.push_back(parse(elem_json["content"]));
        }
    }
    ConcatElement::ConcatElement(
        const std::vector<Ptr> &elems,
        const std::vector<InfoHandle> &assoc_infos
    ) : DisplayElement(assoc_infos), elems(elems) {}

    Ptr ConcatElement::copy() const {
        std::vector<Ptr> new_elems;
        for (auto &elem : elems) {
            new_elems.push_back(elem->copy());
        }
        return std::make_shared<ConcatElement>(new_elems, assoc_infos);
    }

    std::string ConcatElement::to_text(DataHandle dh) const {
        std::string res;
        for (auto &elem : elems) {
            res += elem->to_text(dh);
        }
        return res;
    }

    json ConcatElement::show(DataHandle dh) const {
        json res;
        for (auto &el : elems) {
            res.push_back(el->show(dh));
        }
        return res;
    }

    std::shared_ptr<Component> ConcatElement::toComponent(CreationContext &creation_context) {
        if (!this->generated_component) {
            std::vector<std::shared_ptr<Component>> sons;
            transform(this->elems.begin(), this->elems.end(), sons.begin(),
                [&creation_context](const Ptr &son) {
                return son->toComponent(creation_context);
            });
            this->generated_component = static_pointer_cast<Component>(make_shared<ConcatComponent>(sons));
        }
        return this->generated_component;
    }


    // ---------------- StartLineElement ---------------- //

    StartLineElement::StartLineElement(const json &elem_json) : DisplayElement({}) {
        ASSUME_HAS(elem_json, "type");
        ASSUME_VAL(elem_json, "type", "start_line");
        if (elem_json.contains("number")) {
            ASSUME_HAS_UINT_ASSIGN(elem_json, number);
        }
    }
    StartLineElement::StartLineElement(const StartLineElement &other) :
        DisplayElement(other.assoc_infos), number(other.number) {}

    Ptr StartLineElement::copy() const {
        return std::make_shared<StartLineElement>(*this);
    }

    std::string StartLineElement::to_text(DataHandle dh) const {
        return "\n";
    }

    json StartLineElement::show(DataHandle dh) const {
        json res;
        res["type"] = "start_line";
        if (number.has_value()) {
            res["number"] = number.value();
        }
        return res;
    }

    std::shared_ptr<Component> StartLineElement::toComponent(CreationContext &creation_context) {
        if (!this->generated_component) {
            this->generated_component = static_pointer_cast<Component>(make_shared<StartLineComponent>(this->number));
        }
        return this->generated_component;
    }


    // ---------------- InteractElement ---------------- //
    
    InteractElement::InteractElement(const json &elem_json) : DisplayElement({}) {
        // Both "entity" and "grouping" syntax elements can be
        // interactive.
        // Note that entity data is lost when creating an instance
        // of InteractElement - that is why entities are recognised
        // before interacts upon parsing.
        ASSUME_HAS(elem_json, "type");
        ASSUME(elem_json["type"] == "entity" || elem_json["type"] == "grouping", "interactive element is neither entity nor a grouping");

        ASSUME_HAS(elem_json, "content");
        content = parse(elem_json["content"]);
        ASSUME_HAS(elem_json, "alt_content");
        alt_content = parse(elem_json["alt_content"]);
    }
    InteractElement::InteractElement(
        Ptr content, Ptr alt_content,
        const std::vector<InfoHandle> &assoc_infos
    ) : DisplayElement(assoc_infos), content(content), alt_content(alt_content) {}

    Ptr InteractElement::copy() const {
        return std::make_shared<InteractElement>(content->copy(), alt_content->copy(), assoc_infos);
    }

    std::string InteractElement::to_text(DataHandle dh) const {
        return content->to_text(dh);
    }

    json InteractElement::show(DataHandle dh) const {
        json res;
        res["type"] = "interact";
        res["content"] = content->show(dh);
        res["alt_content"] = alt_content->show(dh);
        return res;
    }

    std::shared_ptr<Component> InteractElement::toComponent(CreationContext &creation_context) {
        if (!this->generated_component) {
            auto primary = this->content->toComponent(creation_context);
            auto alternative = this->alt_content->toComponent(creation_context);
            auto result = make_shared<InteractiveComponent>(view_manager::getNewId(), primary, alternative);
            creation_context.id_to_interactive_component->emplace(result->getId(), std::weak_ptr<InteractiveComponent>(result));
            this->generated_component = static_pointer_cast<Component>(result);
        }
        return this->generated_component;
    }


    // ---------------- LazyElement ---------------- //

    LazyElement::LazyElement(const json &elem_json) : DisplayElement({}) {
        ASSUME_HAS(elem_json, "type");
        ASSUME_VAL(elem_json, "type", "lazy");
        ASSUME_HAS(elem_json, "handle");
        handle = elem_json["handle"];
    }
    LazyElement::LazyElement(const LazyElement &other) :
        DisplayElement(other.assoc_infos), handle(other.handle) {}

    Ptr LazyElement::copy() const {
        return std::make_shared<LazyElement>(*this);
    }

    std::string LazyElement::to_text(DataHandle dh) const {
        return evaluated(nullptr, dh)->to_text(dh);
    }

    Ptr LazyElement::evaluated(Ptr _, DataHandle dh) const {
        Ptr res = fetch_resource(handle, dh);
        // Associated infos are passed onto the newly fetched
        // resource.
        for (auto info : assoc_infos) {
            res->add_assoc_info(info);
        }
        return res;
    }

    json LazyElement::show(DataHandle dh) const {
        return evaluated(nullptr, dh)->show(dh);
    }
    

    // ---------------- EntityElement ---------------- //

    EntityElement::EntityElement(const json &elem_json) : DisplayElement({}) {
        ASSUME_HAS(elem_json, "type");
        ASSUME_VAL(elem_json, "type", "entity");
        ASSUME_HAS_STR_ASSIGN(elem_json, refers_to);

        // Entities may be interactive in the diagnostics
        // file syntax.
        if (elem_json.contains("alt_content")) {
            content = std::make_shared<InteractElement>(elem_json);
        } else {
            ASSUME_HAS(elem_json, "content");
            content = parse(elem_json["content"]);
        }
    }
    EntityElement::EntityElement(
        const std::string &refers_to, Ptr content,
        const std::vector<InfoHandle> &assoc_infos
    ) : DisplayElement(assoc_infos), refers_to(refers_to), content(content) {}

    Ptr EntityElement::copy() const {
        return std::make_shared<EntityElement>(refers_to, content->copy(), assoc_infos);
    }

    std::vector<InfoHandle> EntityElement::get_assoc_infos(DataHandle dh) const {
        auto res = assoc_infos;
        // Scan the entity metadata for associated infos
        // and fetch all the necessary resources along the way.
        ASSUME_HAS(dh.entities, refers_to);
        ASSUME_HAS_STR(dh.entities[refers_to], "kind");
        auto &entity = dh.entities[refers_to];

        // If the referred entity is not fetched, do it now.
        if (entity["kind"] == "lazy_entity") {
            ASSUME_HAS(entity, "handle");
            fetch_entity({refers_to, entity["handle"]}, dh);
        }
        // Scan the entity's metadata for associated infos.
        auto entity_res = scan_entity_metadata(dh.entities[refers_to], dh);

        // Merge the two info pools.
        for (auto info : entity_res) {
            if (std::find(res.begin(), res.end(), info) == res.end()) {
                res.push_back(info);
            }
        }
        return res;
    }

    std::string EntityElement::to_text(DataHandle dh) const {
        return content->to_text(dh);
    }

    json EntityElement::show(DataHandle dh) const {
        json res;
        res["type"] = "entity";
        res["refers_to"] = refers_to;
        res["content"] = content->show(dh);
        return res;
    }


    // ---------------- CodeElement ---------------- //

    CodeElement::CodeElement(const json &elem_json) : DisplayElement({}) {
        ASSUME_HAS(elem_json, "type");
        ASSUME_VAL(elem_json, "type", "code");
        ASSUME_HAS(elem_json, "content");
        content = parse(elem_json["content"]);
    }
    CodeElement::CodeElement(
        Ptr content, const std::vector<InfoHandle> &assoc_infos
    ) : DisplayElement(assoc_infos), content(content) {}

    Ptr CodeElement::copy() const {
        return std::make_shared<CodeElement>(content->copy(), assoc_infos);
    }

    std::string CodeElement::to_text(DataHandle dh) const {
        return content->to_text(dh);
    }

    json CodeElement::show(DataHandle dh) const {
        json res;
        res["type"] = "code";
        res["content"] = content->show(dh);
        return res;
    }

    std::shared_ptr<Component> CodeElement::toComponent(CreationContext &creation_context) {
        if (!this->generated_component) {
            this->generated_component = static_pointer_cast<Component>(make_shared<CodeElement>(content));
        }
        return this->generated_component;
    }

    // ---------------- functions ---------------- //

    /* Order of message type evaluation:
        handle -> entity -> interact -> start_line -> concat -> text */
    Ptr parse(const json &msg) {
        if (msg.contains("type") && msg["type"] == "lazy") {
            return std::make_shared<LazyElement>(msg);
        }
        if (msg.contains("type") && msg["type"] == "entity") {
            return std::make_shared<EntityElement>(msg);
        }
        if (msg.contains("alt_content")) {
            return std::make_shared<InteractElement>(msg);
        }
        if (msg.contains("type") && msg["type"] == "start_line") {
            return std::make_shared<StartLineElement>(msg);
        }
        if (msg.contains("type") && msg["type"] == "code") {
            return std::make_shared<CodeElement>(msg);
        }
        if (msg.is_array() || (msg.contains("type") && msg["type"] == "grouping")) {
            return std::make_shared<ConcatElement>(msg);
        }
        return std::make_shared<TextElement>(msg);
    }

    Ptr fetch_resource(ResourceHandle resource_handle, DataHandle data_handle) {
        // Here fetching from the LS may happen in the future
        // which may modify the data under data_handle.
        return parse(resource_handle);
    }

    void fetch_entity(EntityHandle entity_handle, DataHandle data_handle) {
        // Here fetching from the LS may happen in the future
        // which may modify the data under data_handle.
        data_handle.entities[entity_handle.first] = entity_handle.second;
    }

    std::vector<InfoHandle> scan_entity_metadata(const json &entity, DataHandle handle) {
        // Note: during scanning it may be necessary to fetch
        //       some other entities - that's what the data handle is for.
        std::vector<InfoHandle> res;
        
        // Definition scan.
        if (entity.contains("defined_at")) {
            ASSUME_UINT(entity, "defined_at");
            res.push_back(InfoParamsHandle::add(entity["defined_at"], handle));
        }
        // TODO: more functionalities may be added here.
        
        return res;
    }

} // namespace dia_file
} // namespace dia_app