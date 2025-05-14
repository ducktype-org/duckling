#include "components.hpp"

#include <proto/view.pb.h>

namespace dia_app {
namespace view_manager {
    // Component

    Component::~Component() {}

    std::shared_ptr<Component> Component::deepCopy() {
        return {};
    }

    std::vector<::view::Component> Component::getView() const {
        return {};
    }

    void Component::registerInteraction(InteractionType interaction_type) {
        auto strong_parent = parent.lock();
        if (strong_parent) {
            strong_parent->registerInteraction(interaction_type);
        }
    }

    // TextComponent

    TextComponent::TextComponent(std::string content) : content(std::move(content)) {}

    std::vector<::view::Component> TextComponent::getView() const {
        ::view::Component result;
        result.mutable_text_component()->set_content(this->content);
        return {result};
    }

    std::shared_ptr<Component> TextComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<TextComponent>(this->content));
    }

    // CodeComponent

    CodeComponent::CodeComponent(std::string content) : content(std::move(content)) {}

    std::vector<::view::Component> CodeComponent::getView() const {
        ::view::Component result;
        result.mutable_code_component()->set_content(this->content);
        return {result};
    }

    std::shared_ptr<Component> CodeComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<CodeComponent>(this->content));
    }

    // ConcatComponent

    ConcatComponent::ConcatComponent(std::vector<std::shared_ptr<Component>> components) : components(std::move(components)) {}

    std::vector<::view::Component> ConcatComponent::getView() const {
        std::vector<::view::Component> result;
        if (components.empty()) {
            return result;
        }
        std::vector<std::vector<::view::Component>> sons_results;
        transform(components.begin(), components.end(), sons_results.begin(),
        [](const std::shared_ptr<Component> &component) {
            return component->getView();
        });
        std::vector<std::vector<::view::Component>> results_by_lines;
        results_by_lines.resize(0);
        for (auto elm : sons_results) {
            if (!elm.empty()) {
                results_by_lines.back().insert(results_by_lines.back().end(), *elm.begin());
                transform(elm.begin() + 1, elm.end(), results_by_lines.end(),
                [](const ::view::Component &elm) {
                    return std::vector<::view::Component>{elm};
                });
            }
        }
        transform(results_by_lines.begin(), results_by_lines.end(), result.begin(),
            [](const std::vector<::view::Component> &elms) {
            ::view::Component result;
            result.mutable_concat_component()->mutable_components()->Add(elms.begin(), elms.end());
            return result;
        });
        return result;
    }

    std::shared_ptr<Component> ConcatComponent::deepCopy() {
        std::vector<std::shared_ptr<Component>> new_components;
        transform(this->components.begin(), this->components.end(), new_components.begin(),
            [](const std::shared_ptr<Component> &component) {
            return component->deepCopy();
        });
        return std::static_pointer_cast<Component>(make_shared<ConcatComponent>(new_components));
    }

    // InteractiveComponent

    component_id_t InteractiveComponent::getId() { return this->id; }

    InteractiveComponent::InteractiveComponent(component_id_t id, const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative) :
        id(id), visible(primary->deepCopy()), primary(primary), alternative(alternative) {}
        
    std::unique_ptr<InteractiveComponent> InteractiveComponent::create(const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative) {
        return make_unique<InteractiveComponent>(getNewId(), primary, alternative);
    }

    std::vector<::view::Component> InteractiveComponent::getView() const {
        auto visible_result = visible->getView();
        std::vector<::view::Component> wrapped_results;
        transform(visible_result.begin(), visible_result.end(), wrapped_results.begin(),
            [this](::view::Component &component) {
            ::view::Component result;
            result.mutable_interactive_component()->set_component_id(this->id);
            result.mutable_interactive_component()->set_allocated_primary_component(&component);
            return result;
        });
        return wrapped_results;
    }

    std::shared_ptr<Component> InteractiveComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<InteractiveComponent>(
            getNewId(),
            this->primary,
            this->alternative
        ));
    }

    void InteractiveComponent::registerInteraction(InteractionType interaction_type) {
        if (this->status == Status::Primary && interaction_type == InteractionType::Click) {
            this->visible = alternative->deepCopy();
            this->status = Status::Alternative;
        }
        else if (this->status == Status::Alternative && interaction_type == InteractionType::CtrlClick) {
            this->visible = primary->deepCopy();
            this->status = Status::Primary;
        }
        else {
            return Component::registerInteraction(interaction_type);
        }
    }

    // Section

    Section::~Section() {}

    ::view::Section Section::getView() const {
        return {};
    }

    // TextSection

    TextSection::TextSection(std::shared_ptr<Component> root) : root(std::move(root)) {}

    ::view::TextSection TextSection::getOwnView() const {
        auto lines = this->root->getView();
        ::view::Component concatenated_lines;
        concatenated_lines.mutable_concat_component()->mutable_components()->Add(lines.begin(), lines.end());
        ::view::TextSection result;
        result.set_allocated_root(&concatenated_lines);
        return result;
    }


    ::view::Section TextSection::getView() const {
        ::view::Section result;
        auto tmp = this->getOwnView();
        result.set_allocated_text_section(&tmp);
        return result;
    }

    // CodeSection

    CodeSection::CodeSection(std::shared_ptr<Component> root) : root(std::move(root)) {}

    ::view::Section CodeSection::getView() const {
        auto lines = this->root->getView();
        std::vector<::view::CodeLine> code_lines
        vector<::view::CodeLine> tmp;
        transform(this->lines.begin(), this->lines.end(), tmp.begin(),
            [](const CodeLine &line) {
            return line.getView();
        });
        ::view::Section result;
        result.mutable_code_section()->mutable_lines()->Add(tmp.begin(), tmp.end());
        return result;
    }

    // NoHlTextSection

    ::view::Section NoHlTextSection::getView() const {
        auto tmp = this->root->getNoHlView();
        ::view::Section result;
        result.mutable_no_hl_text_section()->set_allocated_root(&tmp);
    }
}
}
