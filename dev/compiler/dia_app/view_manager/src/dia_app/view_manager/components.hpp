#pragma once
#include <bits/stdc++.h>
#include <proto/view.pb.h>

using namespace std;

using component_id_t = int32_t;

enum class InteractionType : uint8_t {
    Click,
    ShiftClick,
    CtrlClick
};;

class Component {
    private:

    public:
    weak_ptr<Component> parent;

    ::view::Component getView() const;
    ::view::NoHlComponent getNoHlView() const;

    virtual void registerInteraction(InteractionType interaction_type) {
        auto strong_parent = parent.lock();
        if (strong_parent) {
            strong_parent->registerInteraction(interaction_type);
        }
    }

    virtual ~Component() {}

    virtual shared_ptr<Component> deepCopy();
};

class TextComponent : public Component {
    private:
    string content;

    public:

    TextComponent(string content) : content(std::move(content)) {}
    ::view::Component getView() const {
        ::view::Component result;
        result.mutable_text_component()->set_content(this->content);
        return result;
    }

    shared_ptr<Component> deepCopy() override {
        return std::static_pointer_cast<Component>(make_shared<TextComponent>(this->content));
    }
};

class CodeComponent : public Component {
    private:
    string content;

    public:

    CodeComponent(string content) : content(std::move(content)) {}
    ::view::Component getView() const {
        ::view::Component result;
        result.mutable_code_component()->set_content(this->content);
        return result;
    }

    shared_ptr<Component> deepCopy() override {
        return std::static_pointer_cast<Component>(make_shared<CodeComponent>(this->content));
    }
};

class ConcatComponent : public Component {
    private:
    vector<shared_ptr<Component>> components;


    public:
    ConcatComponent(vector<shared_ptr<Component>> components) : components(std::move(components)) {}
    ::view::Component getView() const {
        ::view::Component result;
        vector<::view::Component> tmp;
        transform(components.begin(), components.end(), tmp.begin(),
        [](const shared_ptr<Component> &component) {
            return component->getView();
        });
        result.mutable_concat_component()->mutable_components()->Add(tmp.begin(), tmp.end());
        return result;
    }

    shared_ptr<Component> deepCopy() override {
        vector<shared_ptr<Component>> new_components;
        transform(this->components.begin(), this->components.end(), new_components.begin(),
            [](const shared_ptr<Component> &component) {
            return component->deepCopy();
        });
        return std::static_pointer_cast<Component>(make_shared<ConcatComponent>(new_components));
    }
};

inline component_id_t getNewId() {
    static component_id_t next = 0;
    return next++;
}

class InteractiveComponent : public Component {
    private:
    enum class Status : bool {
        Primary,
        Alternative
    };

    component_id_t id;
    Status status = Status::Primary;
    shared_ptr<Component> visible, primary, alternative;

    public:
    InteractiveComponent(component_id_t id, const shared_ptr<Component>& primary, const shared_ptr<Component>& alternative) :
        id(id), visible(primary->deepCopy()), primary(primary), alternative(alternative) {}
        
    unique_ptr<InteractiveComponent> create(const shared_ptr<Component>& primary, const shared_ptr<Component>& alternative) {
        return make_unique<InteractiveComponent>(getNewId(), primary, alternative);
    }

    ::view::Component getView() const {
        ::view::Component result;
        auto tmp = visible->getView();
        result.mutable_interactive_component()->set_allocated_primary_component(&tmp);
        result.mutable_interactive_component()->set_component_id(id);
        return result;
    }

    shared_ptr<Component> deepCopy() override {
        return std::static_pointer_cast<Component>(make_shared<InteractiveComponent>(
            getNewId(),
            this->primary,
            this->alternative
        ));
    }

    void registerInteraction(InteractionType interaction_type) override {
        if (this->status == Status::Primary && interaction_type == InteractionType::Click) {
            this->visible = alternative->deepCopy();
            this->status = Status::Alternative;
        }
        else if (this->status == Status::Alternative && interaction_type == InteractionType::CtrlClick) {
            this->visible = primary->deepCopy();
            this->status = Status::Primary;
        }
        else {
            return ::Component::registerInteraction(interaction_type);
        }
    }
};

class StartLineComponent : public Component {
    private:
    std::optional<uint> number;

    public:
    StartLineComponent(std::optional<uint> number) : number(number) {}
    StartLineComponent() {}
    StartLineComponent(uint number) : number(number) {}
};

class Section {
    private:

    public:
    virtual ~Section();
    virtual ::view::Section getView() const;
};

class TextSection : public Section {
    private:
    shared_ptr<Component> root;

    public:
    TextSection(shared_ptr<Component> root) : root(std::move(root)) {}
    ::view::TextSection getOwnView() const {
        auto tmp = this->root->getView();
        ::view::TextSection result;
        result.set_allocated_root(&tmp);
        return result;
    }


    virtual ::view::Section getView() const override {
        ::view::Section result;
        auto tmp = this->getOwnView();
        result.set_allocated_text_section(&tmp);
        return result;
    }
};

class CodeLine {
    private:
    unique_ptr<TextSection> root;
    optional<int32_t> line_number;

    public:

    ::view::CodeLine getView() const {
        ::view::CodeLine result;
        auto tmp = this->root->getOwnView();
        result.set_allocated_content(&tmp);
        if (this->line_number.has_value()) {
            result.set_line_number(this->line_number.value());
        }
        return result;
    }
};

class CodeSection : public Section {
    private:
    shared_ptr<Component> root;

    public:
    CodeSection(shared_ptr<Component> root) : root(std::move(root)) {}
    // virtual ::view::Section getView() const override {
    //     vector<::view::CodeLine> tmp;
    //     transform(this->lines.begin(), this->lines.end(), tmp.begin(),
    //         [](const CodeLine &line) {
    //         return line.getView();
    //     });
    //     ::view::Section result;
    //     result.mutable_code_section()->mutable_lines()->Add(tmp.begin(), tmp.end());
    //     return result;
    // }
};

class NoHlTextSection : public Section {
    private:
    unique_ptr<Component> root;

    public:
    virtual ::view::Section getView() const override {
        auto tmp = this->root->getNoHlView();
        ::view::Section result;
        result.mutable_no_hl_text_section()->set_allocated_root(&tmp);
    }
};

struct CreationContext {
    std::unique_ptr<std::unordered_map<component_id_t, std::weak_ptr<InteractiveComponent>>> id_to_interactive_component;
//     std::vector<CodeLine> lines;
};

// class CreationElement {
//     public:
//     std::shared_ptr<Component> created = shared_ptr<Component>();
// };