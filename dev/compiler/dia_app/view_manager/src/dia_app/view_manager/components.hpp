#pragma once
#include <proto/view.pb.h>
#include <memory>
#include "utils.hpp"

namespace dia_app {
namespace view_manager {
    using component_id_t = uint32_t;

    using side_entry_id_t = uint32_t;
    using side_info_id_t = uint32_t;

    using hl_id_t = uint32_t;
    using priority_t = uint32_t;

    using line_no_t = uint32_t;
    using column_no_t = uint32_t;


    using line_metadata_t = std::optional<uint>;

    template<class T>
    using line_suffix_data_t = std::optional<std::unique_ptr<T>>;

    template<class T>
    using line_data_t = std::pair<line_metadata_t, std::optional<std::unique_ptr<T>>>;

    template<class T>
    using component_get_view_data_t = std::pair<line_suffix_data_t<T>, std::vector<line_data_t<T>>>;


    enum class InteractionType : uint8_t {
        Click,
        ClickInteractive,
        ClickInteractiveRollback
    };;

    inline std::string print(InteractionType it) {
        switch (it) {
            case dia_app::view_manager::InteractionType::Click: return "Click";
            case dia_app::view_manager::InteractionType::ClickInteractive: return "ClickInteractive";
            case dia_app::view_manager::InteractionType::ClickInteractiveRollback: return "ClickInteractiveRollback";
        }
        assert(false);
    };

    class Component {
        private:

        public:
        std::weak_ptr<Component> parent;

        virtual component_get_view_data_t<::view::HlComponent> getHlView() const;

        virtual component_get_view_data_t<::view::NoHlComponent> getNoHlView() const;

        virtual void registerInteraction(InteractionType interaction_type);

        virtual ~Component();

        virtual std::shared_ptr<Component> deepCopy();
    };

    class TextComponent : public Component {
        private:
        std::string content;
        std::vector<side_entry_id_t> assoc_side_entries;

        public:

        TextComponent(std::string content, std::vector<side_entry_id_t> accos_side_entries);

        component_get_view_data_t<::view::NoHlComponent> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    class CodeComponent : public Component {
        private:
        std::string content;
        std::vector<hl_id_t> tags;
        std::vector<side_info_id_t> assoc_side_entries;

        public:

        CodeComponent(std::string content, std::vector<hl_id_t> tags, std::vector<side_entry_id_t> assoc_side_infos);

        component_get_view_data_t<::view::HlComponent> getHlView() const override;

        component_get_view_data_t<::view::NoHlComponent> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    class ConcatComponent : public Component {
        private:
        std::vector<std::shared_ptr<Component>> components;

        public:
        ConcatComponent(std::vector<std::shared_ptr<Component>> components);

        component_get_view_data_t<::view::HlComponent> getHlView() const override;

        component_get_view_data_t<::view::NoHlComponent> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    inline component_id_t getNewId() {
        static component_id_t next = 0;
        return next++;
    }

    class InteractiveComponent;
    using id_to_interactive_component_mapping_t = std::unordered_map<component_id_t, std::weak_ptr<InteractiveComponent>>;
    class InteractiveComponent : public Component {
        public:
        enum class Status : bool {
            Primary,
            Alternative
        };

        private:
        component_id_t id;
        Status status = Status::Primary;
        std::shared_ptr<Component> visible, primary, alternative;
        std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component;

        public:
        component_id_t getId();

        InteractiveComponent(component_id_t id, const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative, std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component);
            
        component_get_view_data_t<::view::HlComponent> getHlView() const override;

        component_get_view_data_t<::view::NoHlComponent> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;

        void registerInteraction(InteractionType interaction_type) override;
    };

    struct CreationContext {
        std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component;
        std::unique_ptr<std::map<std::string, hl_id_t>> hl_name_to_id;
        std::optional<DataHandle> data_handle;
    };

    class StartLineComponent : public Component {
        private:
        std::optional<uint> number;

        public:
        StartLineComponent(std::optional<uint> number);

        component_get_view_data_t<::view::HlComponent> getHlView() const override;

        component_get_view_data_t<::view::NoHlComponent> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };
} // namespace view_manager
} // namespace dia_app
