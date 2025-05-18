#pragma once
#include "view_constructor.hpp"

#include <json/json.hpp>
#include <proto/view.grpc.pb.h>
#include <proto/view.pb.h>

namespace dia_app {
namespace view_manager {
    using dia_app::message_template::Info;
    using dia_app::ViewConstructor;

    using nlohmann::json;

    class Metadata {
        private:
        std::optional<std::string> error_code, file_info;

        public:
        Metadata(std::optional<std::string> error_code, std::optional<std::string> file_info);

        static Metadata createFromInfo(const Info &info);

        std::unique_ptr<::view::Metadata> getView() const;
    };


    class HlInfo {
        private:
        hl_id_t tag;
        std::string message;

        public:
        HlInfo(hl_id_t tag, std::string message);

        std::unique_ptr<::view::HlInfo> getView() const;
    };

    class Diagnostic {
        private:
        Metadata metadata;
        std::vector<std::shared_ptr<Section>> sections;
        std::vector<HlInfo> hl_messages;

        public:
        Diagnostic(Metadata metadata, std::vector<std::shared_ptr<Section>> sections, std::vector<HlInfo> hl_messages);

        static Diagnostic createFromInfo(const Info &info, CreationContext &creation_context);

        std::unique_ptr<::view::Diagnostic> getView() const;
    };

    class ViewManager {
        private:
        std::vector<Diagnostic> diagnostics;
        std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component;

        public:
        ViewManager(std::vector<Diagnostic> diagnostics, std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component);

        static ViewManager createFromJson(const json &input);

        void getView(::view::ViewResponse* response);

        void registerInteraction(component_id_t id, InteractionType interaction_type);
    };

    class ViewServiceImpl : public ::view::ViewService::Service {
        private:
        ViewManager vm;

        public:
        ViewServiceImpl(ViewManager vm);

        static ViewServiceImpl createFromJson(const json &input);

        ::grpc::Status GetView(::grpc::ServerContext* context,
                            const ::view::ViewRequest* request,
                            ::view::ViewResponse* response) override;

        ::grpc::Status Click(::grpc::ServerContext* context,
                            const ::view::ClickRequest* request,
                            ::view::ClickResponse* response) override;

        ::grpc::Status CloseSideNote(::grpc::ServerContext* context,
                            const ::view::CloseSideNoteRequest* request,
                            ::view::CloseSideNoteResponse* response) override;
    };

    void runViewManagerRPCServer(const json &input);
}
}
