#pragma once
#include "utils.hpp"
#include <proto/view.pb.h>
#include <memory>

namespace dia_app {
namespace view_manager {
    using component_id_t = int32_t;
    using line_id_t = int32_t;
    using hl_id_t = int32_t;

    using line_metadata_t = std::optional<uint>;

    template<class T>
    using ptr = T*;

    template<class T>
    using line_data_t = std::pair<line_metadata_t, T>;

    enum class InteractionType : uint8_t {
        Click,
        ShiftClick,
        CtrlClick
    };;

    class Component {
        private:

        public:
        std::weak_ptr<Component> parent;

        virtual std::vector<line_data_t<ptr<::view::Component>>> getView() const;

        virtual std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const;

        virtual void registerInteraction(InteractionType interaction_type);

        virtual ~Component();

        virtual std::shared_ptr<Component> deepCopy();
    };

    class TextComponent : public Component {
        private:
        std::string content;
        std::vector<hl_id_t> tags;

        public:

        TextComponent(std::string content, std::vector<hl_id_t> tags);

        std::vector<line_data_t<ptr<::view::Component>>> getView() const override;
        
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    class CodeComponent : public Component {
        private:
        std::string content;
        std::vector<hl_id_t> tags;

        public:

        CodeComponent(std::string content, std::vector<hl_id_t> tags);

        std::vector<line_data_t<ptr<::view::Component>>> getView() const override;
        
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    class ConcatComponent : public Component {
        private:
        std::vector<std::shared_ptr<Component>> components;
        std::vector<hl_id_t> tags;

        public:
        ConcatComponent(std::vector<std::shared_ptr<Component>> components, std::vector<hl_id_t> tags);

        std::vector<line_data_t<ptr<::view::Component>>> getView() const override;
        
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
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
        std::shared_ptr<Component> visible, primary, alternative;

        public:
        component_id_t getId();

        InteractiveComponent(component_id_t id, const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative);
            
        std::unique_ptr<InteractiveComponent> create(const std::shared_ptr<Component>& primary, const std::shared_ptr<Component>& alternative);

        std::vector<line_data_t<ptr<::view::Component>>> getView() const override;
        
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;

        void registerInteraction(InteractionType interaction_type) override;
    };

    class StartLineComponent : public Component {
        private:
        std::optional<uint> number;

        public:
        StartLineComponent(std::optional<uint> number);

        std::vector<line_data_t<ptr<::view::Component>>> getView() const override;
        
        std::vector<line_data_t<ptr<::view::NoHlComponent>>> getNoHlView() const override;

        std::shared_ptr<Component> deepCopy() override;
    };

    class Section {
        private:

        public:
        virtual ~Section();
        virtual ptr<::view::Section> getView() const;
    };

    class TextSection : public Section {
        private:
        std::shared_ptr<Component> root;

        public:
        TextSection(std::shared_ptr<Component> root);

        ptr<::view::TextSection> getOwnView() const;

        virtual ptr<::view::Section> getView() const override;
    };
    
    class CodeSection : public Section {
        private:
        std::shared_ptr<Component> root;

        public:
        CodeSection(std::shared_ptr<Component> root);
        virtual ptr<::view::Section> getView() const override;
    };

    class NoHlTextSection : public Section {
        private:
        std::unique_ptr<Component> root;

        public:
        virtual ptr<::view::Section> getView() const override;
    };

    struct CreationContext {
        std::unique_ptr<std::unordered_map<component_id_t, std::weak_ptr<InteractiveComponent>>> id_to_interactive_component;
        std::unique_ptr<std::map<std::string, hl_id_t>> hl_name_to_id;
        dia_app::DataHandle data_handle;
    };
}
}
