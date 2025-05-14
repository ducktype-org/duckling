#include "view_constructor.hpp"

#include <bits/stdc++.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server.h>
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
        static Metadata createFromInfo(const Info &info) {

        }

        ::view::Metadata getView() const {
            ::view::Metadata result;
            if (this->error_code.has_value()) {
                result.set_error_code(this->error_code.value());
            }
            if (this->file_info.has_value()) {
                result.set_file_info(this->file_info.value());
            }
            return result;
        }
    };


    class HlInfo {
        private:

        public:
        ::view::HlInfo getView() const;
    };

    class Diagnostic {
        private:
        Metadata metadata;
        std::vector<Section> sections;
        std::vector<HlInfo> hl_messages;

        public:
        Diagnostic(Metadata metadata, std::vector<Section> sections, std::vector<HlInfo> hl_messages) : metadata(metadata), sections(sections), hl_messages(hl_messages) {}
        static Diagnostic createFromInfo(const Info &info, CreationContext &creation_context) {
            auto metadata = Metadata::createFromInfo(info);
            std::vector<Section> sections;
            // if (info.header_message) {
            //     sections.emplace_back(TextSection(info.header_message->toComponent(creation_context)));
            // }
            // if (info.code.has_value()) {
            //     sections.emplace_back(CodeSection(info.code->content->toComponent(creation_context)));
            // }
            // if (info.description) {
            //     sections.emplace_back(TextSection(info.description->toComponent(creation_context)));
            // }
            std::vector<HlInfo> hl_messages;
            return Diagnostic(metadata, sections, hl_messages);
        }

        ::view::Diagnostic getView() const {
            ::view::Diagnostic diagnostic;
            // {
            //     auto metadata = this->metadata.getView();
            //     diagnostic.set_allocated_metadata(&metadata);
            // }
            // {
            //     std::vector<::view::Section> tmp;
            //     transform(sections.begin(), sections.end(), tmp.begin(),
            //         [](const Section &section) {
            //         return section.getView();
            //     });
            //     diagnostic.mutable_sections()->Add(tmp.begin(), tmp.end());
            // }
            // {
            //     std::vector<::view::HlInfo> tmp;
            //     transform(hl_messages.begin(), hl_messages.end(), tmp.begin(),
            //         [](const HlInfo &hl_info) {
            //         return hl_info.getView();
            //     });
            //     diagnostic.mutable_hl_messages()->Add(tmp.begin(), tmp.end());
            // }
            return diagnostic;
        }
    };

    class ViewManager {
        private:
        std::vector<Diagnostic> diagnostics;
        // unique_ptr<unordered_map<component_id_t, weak_ptr<InteractiveComponent>>> id_to_interactive_component;


        public:
        ViewManager(std::vector<Diagnostic> diagnostics) : diagnostics(std::move(diagnostics)) {}
        static ViewManager createFromJson(const json &input) {
            // TODO: WTF?
            ViewConstructor view_constructor(0, input);

            Info info = *view_constructor.load_main_info();

            auto creation_context = CreationContext{.id_to_interactive_component=std::make_unique<std::unordered_map<component_id_t, std::weak_ptr<InteractiveComponent>>>()};

            auto diagnostics = std::vector<Diagnostic>{Diagnostic::createFromInfo(info, creation_context)};

            return ViewManager(diagnostics);
        }

        void getView(::view::ViewResponse* response) {
            // std::vector<::view::Diagnostic> tmp;
            // transform(diagnostics.begin(), diagnostics.end(), tmp.begin(),
            // [](const Diagnostic &diagnostic) {
            //     return diagnostic.getView();
            // });
            // response->mutable_diagnostics()->Add(tmp.begin(), tmp.end());
        }

        void registerInteraction(component_id_t id, InteractionType interaction_type) {
            // auto ptr = id_to_interactive_component->find(id);
            // if (ptr == id_to_interactive_component->end()) {
            //     return;
            // }
            // auto component = ptr->second.lock();
            // if (component) {
            //     component->registerInteraction(interaction_type);
            // }
        }
    };

    class ViewServiceImpl : public ::view::ViewService::Service {
        private:
        ViewManager vm;


        public:
        ViewServiceImpl(ViewManager vm) : vm(std::move(vm)) {}
        static ViewServiceImpl createFromJson(const json &input) {
            return ViewServiceImpl(ViewManager::createFromJson(input));
        }

        ::grpc::Status GetView(::grpc::ServerContext* context,
                            const ::view::ViewRequest* request,
                            ::view::ViewResponse* response) override {
            vm.getView(response);
            return ::grpc::Status::OK;
        }

        ::grpc::Status Click(::grpc::ServerContext* context,
                            const ::view::ClickRequest* request,
                            ::view::ClickResponse* response) override {
            InteractionType interaction_type = [&request](){
                switch(request->click_type()) {
                    case ::view::ClickType::CLICK: return InteractionType::Click;
                    case ::view::ClickType::CTRL_CLICK: return InteractionType::CtrlClick;
                    case ::view::ClickType::SHIFT_CLICK: return InteractionType::ShiftClick;
                }
            }();
            vm.registerInteraction(request->component_id(), interaction_type);
            return ::grpc::Status::OK;
        }

        ::grpc::Status CloseSideNote(::grpc::ServerContext* context,
                            const ::view::CloseSideNoteRequest* request,
                            ::view::CloseSideNoteResponse* response) override {
            return ::grpc::Status::OK;
        }
    };

    void runViewManager(const json input) {
        ViewServiceImpl service = ViewServiceImpl::createFromJson(input);
        grpc::ServerBuilder builder;
        builder.AddListeningPort("localhost:50051", grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "ViewManager started on port 50051\n";
        server->Wait();
    }
}
}
