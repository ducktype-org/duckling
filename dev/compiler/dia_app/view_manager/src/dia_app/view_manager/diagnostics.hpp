#pragma once
#include "view_constructor.hpp"

namespace dia_app {
namespace view_manager {
    class CodeMetadata {
        private:
        std::string filename;
        line_no_t line;
        column_no_t column;

        public:
        CodeMetadata(std::string filename, line_no_t line, column_no_t column);

        static std::unique_ptr<CodeMetadata> createFromLocation(const dia_file::CodeData::Location &location);

        std::unique_ptr<::view::CodeMetadata> getView() const;
    };

    class HlMessage {
        private:
        hl_id_t tag;
        priority_t priority;
        std::shared_ptr<Component> content;

        public:
        HlMessage(hl_id_t tag, priority_t priority, std::shared_ptr<Component> content);

        std::unique_ptr<::view::HlMessage> getView() const;
    };

    class Section {
        private:

        public:
        virtual ~Section();

        virtual std::unique_ptr<::view::Section> getView() const;
    };

    class TextSection : public Section {
        private:
        std::shared_ptr<Component> root;

        public:
        TextSection(std::shared_ptr<Component> root);
        
        std::unique_ptr<::view::Section> getView() const override;
    };
    
    class CodeSection : public Section {
        private:
        CodeMetadata code_metadata;
        std::shared_ptr<Component> root;
        std::vector<HlMessage> hl_messages;

        public:
        CodeSection(CodeMetadata code_metadata, std::shared_ptr<Component> root, std::vector<HlMessage> hl_messages);

        static std::unique_ptr<CodeSection> createFromInfo(const message_template::Info &info, CreationContext &creation_context);

        std::unique_ptr<::view::Section> getView() const override;
    };

    using error_code_t = uint32_t;

    enum class InfoType : uint8_t {
        Error,
        Warning,
        Note,
        Hint,
        Docs
    };

    class Metadata {
        private:
        InfoType type;
        error_code_t code;

        public:
        Metadata(InfoType type, error_code_t code);

        static Metadata createFromInfo(const message_template::Info &info);

        std::unique_ptr<::view::Metadata> getView() const;
    };

    class Info {
        private:
        Metadata metadata;
        std::vector<std::unique_ptr<Section>> sections;

        public:
        Info(Metadata metadata, std::vector<std::unique_ptr<Section>> sections);

        static Info createFromInfo(const message_template::Info &info, CreationContext &creation_context);

        std::unique_ptr<::view::Info> getView() const;
    };

    class Diagnostic {
        private:
        std::vector<Info> infos;

        public:
        Diagnostic(std::vector<Info> infos);

        static Diagnostic createFromViewConstructor(ViewConstructor &view_constructor, CreationContext &creation_context);

        std::unique_ptr<::view::Diagnostic> getView() const;
    };
    } // namespace view_manager
} // namespace dia_app
