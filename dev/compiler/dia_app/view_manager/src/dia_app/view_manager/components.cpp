#include "components.hpp"

#include <proto/view.pb.h>

namespace dia_app {
namespace view_manager {
    // Component

    Component::~Component() {}

    std::shared_ptr<Component> Component::deepCopy() {
        return {};
    }

    std::vector<line_data_t<ptr<::view::Component>>> Component::getView() const {
        return {};
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> Component::getNoHlView() const {
        return {};
    }

    void Component::registerInteraction(InteractionType interaction_type) {
        auto strong_parent = parent.lock();
        if (strong_parent) {
            strong_parent->registerInteraction(interaction_type);
        }
    }

    // TextComponent

    TextComponent::TextComponent(std::string content, std::vector<hl_id_t> tags) : content(std::move(content)), tags(std::move(tags)) {}

    std::vector<line_data_t<ptr<::view::Component>>> TextComponent::getView() const {
        ::view::Component *result = new ::view::Component;
        result->mutable_text_component()->set_content(this->content);
        result->mutable_text_component()->mutable_hl_tags()->Add(this->tags.begin(), this->tags.end());
        return {{line_metadata_t{}, result}};
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> TextComponent::getNoHlView() const {
        ::view::NoHlComponent *result = new ::view::NoHlComponent;
        result->mutable_text_component()->set_content(this->content);
        return {{line_metadata_t{}, result}};
    }

    std::shared_ptr<Component> TextComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<TextComponent>(this->content, this->tags));
    }

    // CodeComponent

    CodeComponent::CodeComponent(std::string content, std::vector<hl_id_t> tags) : content(std::move(content)), tags(std::move(tags)) {}

    std::vector<line_data_t<ptr<::view::Component>>> CodeComponent::getView() const {
        ::view::Component* result = new ::view::Component;
        result->mutable_code_component()->set_content(this->content);
        result->mutable_code_component()->mutable_hl_tags()->Add(this->tags.begin(), this->tags.end());
        return {{line_metadata_t{}, result}};
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> CodeComponent::getNoHlView() const {
        ::view::NoHlComponent* result = new ::view::NoHlComponent;
        result->mutable_code_component()->set_content(this->content);
        return {{line_metadata_t{}, result}};
    }

    std::shared_ptr<Component> CodeComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<CodeComponent>(this->content, this->tags));
    }

    // ConcatComponent

    ConcatComponent::ConcatComponent(std::vector<std::shared_ptr<Component>> components,
    std::vector<hl_id_t> tags) : components(std::move(components)), tags(std::move(tags)) {}

    std::vector<line_data_t<ptr<::view::Component>>> ConcatComponent::getView() const {
        std::vector<line_data_t<ptr<::view::Component>>> result;
        if (components.empty()) {
            return result;
        }
        std::vector<std::vector<line_data_t<ptr<::view::Component>>>> sons_results(ssize(components));
        transform(components.begin(), components.end(), sons_results.begin(),
        [](const std::shared_ptr<Component> &component) {
            return component->getView();
        });
        std::vector<std::vector<line_data_t<ptr<::view::Component>>>> results_by_lines;
        results_by_lines.resize(0);
        for (auto elm : sons_results) {
            if (!elm.empty()) {
                results_by_lines.back().insert(results_by_lines.back().end(), *elm.begin());
                results_by_lines.resize(ssize(results_by_lines) + ssize(elm) - 1);
                transform(elm.begin() + 1, elm.end(), results_by_lines.end(),
                [](const line_data_t<ptr<::view::Component>> &elm) {
                    return std::vector<line_data_t<ptr<::view::Component>>>{elm};
                });
            }
        }
        results_by_lines.erase(std::remove_if(results_by_lines.begin(), results_by_lines.end(),
            [](const std::vector<line_data_t<ptr<::view::Component>>> &elms) {
                return elms.empty();
            }),
        results_by_lines.end());
        result.resize(ssize(results_by_lines));
        transform(results_by_lines.begin(), results_by_lines.end(), result.begin(),
            [this](const std::vector<line_data_t<ptr<::view::Component>>> &elms) {
            std::vector<ptr<::view::Component>> sub_components(ssize(elms));
            transform(elms.begin(), elms.end(), sub_components.begin(),
                [](const line_data_t<ptr<::view::Component>> &line) {
                    return line.second;
                });
            assert(!elms.empty());
            line_metadata_t line_metadata = elms[0].first;
            ::view::Component *component = new ::view::Component;
            for (auto tmp : sub_components) {
                component->mutable_concat_component()->mutable_components()->AddAllocated(tmp);
            }
            component->mutable_concat_component()->mutable_hl_tags()->Add(this->tags.begin(), this->tags.end());
            return line_data_t<ptr<::view::Component>>{line_metadata, component};
        });
        return result;
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> ConcatComponent::getNoHlView() const {
        // TODO: dirty copy-paste
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> result;
        if (components.empty()) {
            return result;
        }
        std::vector<std::vector<line_data_t<ptr<::view::NoHlComponent>>>> sons_results(ssize(components));
        transform(components.begin(), components.end(), sons_results.begin(),
        [](const std::shared_ptr<Component> &component) {
            return component->getNoHlView();
        });
        std::vector<std::vector<line_data_t<ptr<::view::NoHlComponent>>>> results_by_lines;
        results_by_lines.resize(0);
        for (auto elm : sons_results) {
            if (!elm.empty()) {
                results_by_lines.back().insert(results_by_lines.back().end(), *elm.begin());
                results_by_lines.resize(ssize(results_by_lines) + ssize(elm) - 1);
                transform(elm.begin() + 1, elm.end(), results_by_lines.end(),
                [](const line_data_t<ptr<::view::NoHlComponent>> &elm) {
                    return std::vector<line_data_t<ptr<::view::NoHlComponent>>>{elm};
                });
            }
        }
        results_by_lines.erase(std::remove_if(results_by_lines.begin(), results_by_lines.end(),
            [](const std::vector<line_data_t<ptr<::view::NoHlComponent>>> &elms) {
                return elms.empty();
            }),
        results_by_lines.end());
        result.resize(ssize(results_by_lines));
        transform(results_by_lines.begin(), results_by_lines.end(), result.begin(),
            [this](const std::vector<line_data_t<ptr<::view::NoHlComponent>>> &elms) {
            std::vector<ptr<::view::NoHlComponent>> sub_components(ssize(elms));
            transform(elms.begin(), elms.end(), sub_components.begin(),
                [](const line_data_t<ptr<::view::NoHlComponent>> &line) {
                    return line.second;
                });
            assert(!elms.empty());
            line_metadata_t line_metadata = elms[0].first;
            ::view::NoHlComponent *component = new ::view::NoHlComponent;
            for (auto tmp : sub_components) {
                component->mutable_concat_component()->mutable_components()->AddAllocated(tmp);
            }
            return line_data_t<ptr<::view::NoHlComponent>>{line_metadata, component};
        });
        return result;
    }

    std::shared_ptr<Component> ConcatComponent::deepCopy() {
        std::vector<std::shared_ptr<Component>> new_components(ssize(this->components));
        transform(this->components.begin(), this->components.end(), new_components.begin(),
            [](const std::shared_ptr<Component> &component) {
            return component->deepCopy();
        });
        return std::static_pointer_cast<Component>(make_shared<ConcatComponent>(new_components, this->tags));
    }

    // InteractiveComponent

    component_id_t InteractiveComponent::getId() { return this->id; }

    InteractiveComponent::InteractiveComponent(component_id_t id, const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative) :
        id(id), visible(primary->deepCopy()), primary(primary), alternative(alternative) {}
        
    std::unique_ptr<InteractiveComponent> InteractiveComponent::create(const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative) {
        return make_unique<InteractiveComponent>(getNewId(), primary, alternative);
    }

    std::vector<line_data_t<ptr<::view::Component>>> InteractiveComponent::getView() const {
        auto visible_result = visible->getView();
        std::vector<line_data_t<ptr<::view::Component>>> wrapped_results(ssize(visible_result));
        transform(visible_result.begin(), visible_result.end(), wrapped_results.begin(),
            [this](line_data_t<ptr<::view::Component>> &elm) {
            auto &[line_metadata, component] = elm;
            ::view::Component result;
            result.mutable_interactive_component()->set_component_id(this->id);
            result.mutable_interactive_component()->set_allocated_primary_component(component);
            return line_data_t<ptr<::view::Component>>{line_metadata, component};
        });
        return wrapped_results;
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> InteractiveComponent::getNoHlView() const {
        auto visible_result = visible->getNoHlView();
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> wrapped_results(ssize(visible_result));
        transform(visible_result.begin(), visible_result.end(), wrapped_results.begin(),
            [this](line_data_t<ptr<::view::NoHlComponent>> &elm) {
            auto &[line_metadata, component] = elm;
            ::view::NoHlComponent result;
            result.mutable_interactive_component()->set_component_id(this->id);
            result.mutable_interactive_component()->set_allocated_primary_component(component);
            return line_data_t<ptr<::view::NoHlComponent>>{line_metadata, component};
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

    // StartLineComponent

    StartLineComponent::StartLineComponent(std::optional<uint> number) : number(number) {}

    std::vector<line_data_t<ptr<::view::Component>>> StartLineComponent::getView() const {
        ::view::Component *nop = new ::view::Component;
        std::string *content = new std::string;
        nop->mutable_text_component()->set_allocated_content(content);
        return std::vector<line_data_t<ptr<::view::Component>>>{{line_metadata_t{}, nop}, {this->number, nop}};
    }

    std::vector<line_data_t<ptr<::view::NoHlComponent>>> StartLineComponent::getNoHlView() const {
        ::view::NoHlComponent *nop = new ::view::NoHlComponent;
        std::string *content = new std::string;
        nop->mutable_text_component()->set_allocated_content(content);
        return std::vector<line_data_t<ptr<::view::NoHlComponent>>>{{line_metadata_t{}, nop}, {this->number, nop}};
    }

    std::shared_ptr<Component> StartLineComponent::deepCopy() {
        return std::static_pointer_cast<Component>(make_shared<StartLineComponent>(
            this->number
        ));
    }


    // Section

    Section::~Section() {}

    ptr<::view::Section> Section::getView() const {
        return {};
    }

    // TextSection

    TextSection::TextSection(std::shared_ptr<Component> root) : root(std::move(root)) {}

    ptr<::view::TextSection> TextSection::getOwnView() const {
        auto lines = this->root->getView();
        std::vector<ptr<::view::Component>> components(ssize(lines));
        transform(lines.begin(), lines.end(), components.begin(),
        [](const line_data_t<ptr<::view::Component>> &line_data) {
            return line_data.second;
        });
        ::view::Component *concatenated_lines = new ::view::Component;
        for (auto tmp : components) {
            concatenated_lines->mutable_concat_component()->mutable_components()->AddAllocated(tmp);
        }
        ::view::TextSection *result = new ::view::TextSection;
        result->set_allocated_root(concatenated_lines);
        return result;
    }


    ptr<::view::Section> TextSection::getView() const {
        ::view::Section *result = new ::view::Section;
        auto tmp = this->getOwnView();
        result->set_allocated_text_section(tmp);
        return result;
    }

    // CodeSection

    CodeSection::CodeSection(std::shared_ptr<Component> root) : root(std::move(root)) {}

    ptr<::view::Section> CodeSection::getView() const {
        auto lines = this->root->getView();
        std::vector<ptr<::view::CodeLine>> code_lines(ssize(lines));
        transform(lines.begin(), lines.end(), code_lines.begin(),
            [](line_data_t<ptr<::view::Component>> &line_data) {
            auto [line_metadata, component] = line_data;
            ::view::TextSection *text_section = new ::view::TextSection;
            text_section->set_allocated_root(component);
            ::view::CodeLine *result = new ::view::CodeLine;
            result->set_allocated_content(text_section);
            if (line_metadata.has_value()) {
                result->set_line_number(line_id_t(line_metadata.value()));
            }
            return result;
        });
        ::view::Section *result = new ::view::Section;
        for (auto tmp : code_lines) {
            result->mutable_code_section()->mutable_lines()->AddAllocated(tmp);
        }
        return result;
    }

    // NoHlTextSection

    ptr<::view::Section> NoHlTextSection::getView() const {
        auto lines = this->root->getNoHlView();
        std::vector<ptr<::view::NoHlComponent>> components(ssize(lines));
        transform(lines.begin(), lines.end(), components.begin(),
        [](const line_data_t<ptr<::view::NoHlComponent>> &line_data) {
            return line_data.second;
        });
        ::view::NoHlComponent *concatenated_lines = new ::view::NoHlComponent;
        for (auto tmp : components) {
            concatenated_lines->mutable_concat_component()->mutable_components()->AddAllocated(tmp);
        }
        ::view::Section *result = new ::view::Section;
        result->mutable_no_hl_text_section()->set_allocated_root(concatenated_lines);
        return result;
    }
}
}
