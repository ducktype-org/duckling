#include "view_manager.hpp"

#include <grpcpp/server_builder.h>
#include <grpcpp/server.h>
#include <grpcpp/grpcpp.h>

#include <utility>

namespace dia_app {
namespace view_manager {

    // Metadata
    Metadata::Metadata(std::optional<std::string> error_code, std::optional<std::string> file_info) : error_code(std::move(error_code)), file_info(std::move(file_info)) {}

    Metadata Metadata::createFromInfo(const Info &info) {
        std::optional<std::string> error_code;
        error_code = info.metadata.code;
        std::optional<std::string> file_info;
        if (info.code.has_value()) {
            file_info = info.code.value().location.file;
        }
        return Metadata(error_code, file_info);
    }

    ptr<::view::Metadata> Metadata::getView() const {
        ::view::Metadata *result = new ::view::Metadata;
        if (this->error_code.has_value()) {
            result->set_error_code(this->error_code.value());
        }
        if (this->file_info.has_value()) {
            result->set_file_info(this->file_info.value());
        }
        return result;
    }

    // HlInfo
    HlInfo::HlInfo(hl_id_t tag, std::string message) : tag(tag), message(std::move(message)) {}
    ptr<::view::HlInfo> HlInfo::getView() const {
        ::view::HlInfo *result = new ::view::HlInfo;
        result->set_message(this->message);
        result->set_tag(this->tag);
        return result;
    }

    // Diagnostic
    using std::cerr;
    using std::endl;

    Diagnostic::Diagnostic(Metadata metadata, std::vector<Section> sections, std::vector<HlInfo> hl_messages) : metadata(std::move(metadata)), sections(std::move(sections)), hl_messages(std::move(hl_messages)) {}
    
    Diagnostic Diagnostic::createFromInfo(const Info &info, CreationContext &creation_context) {
        std::vector<HlInfo> hl_messages;
        {
            hl_id_t id = 0;
            for (const auto &[name, pointer_message] : info.pointer_messages) {
                ++id;
                creation_context.hl_name_to_id->emplace(name, id);
                std::string message = pointer_message.message->to_text(creation_context.data_handle);
                hl_messages.emplace_back(id, message);
            }
        }
        auto metadata = Metadata::createFromInfo(info);
        std::vector<Section> sections;
        if (info.header_message) {
            sections.emplace_back(TextSection(info.header_message->toComponent(creation_context)));
        }
        if (info.code.has_value()) {
            sections.emplace_back(CodeSection(info.code->content->toComponent(creation_context)));
        }
        if (info.description) {
            sections.emplace_back(TextSection(info.description->toComponent(creation_context)));
        }
        cerr << "Number of sections: " << ssize(sections) << endl;
        return Diagnostic(metadata, sections, hl_messages);
    }

    ptr<::view::Diagnostic> Diagnostic::getView() const {
        cerr << "Diagnostic::getView()" << endl;
        ::view::Diagnostic *diagnostic = new ::view::Diagnostic;
        {
            auto metadata = this->metadata.getView();
            diagnostic->set_allocated_metadata(metadata);
        }
        {
            std::vector<ptr<::view::Section>> tmp(ssize(sections));
            transform(sections.begin(), sections.end(), tmp.begin(),
                [](const Section &section) {
                return section.getView();
            });
            cerr << "Whille adding view sections: allocated sections cnt: " << ssize(tmp) << endl;
            for (auto elm : tmp) {
                diagnostic->mutable_sections()->AddAllocated(elm);
            }
        }
        {
            std::vector<ptr<::view::HlInfo>> tmp(ssize(hl_messages));
            transform(hl_messages.begin(), hl_messages.end(), tmp.begin(),
                [](const HlInfo &hl_info) {
                return hl_info.getView();
            });
            for (auto elm : tmp) {
                diagnostic->mutable_hl_messages()->AddAllocated(elm);
            }
        }
        return diagnostic;
    }

    // ViewManager

    ViewManager::ViewManager(std::vector<Diagnostic> diagnostics) : diagnostics(std::move(diagnostics)) {}
    
    ViewManager ViewManager::createFromJson(const json &input) {
        // TODO: WTF?
        ViewConstructor view_constructor(0, input);

        Info info = *view_constructor.load_main_info();

        auto creation_context = CreationContext{.id_to_interactive_component=std::make_unique<std::unordered_map<component_id_t, std::weak_ptr<InteractiveComponent>>>(),
        .hl_name_to_id=std::make_unique<std::map<std::string, hl_id_t>>(),
        .data_handle = view_constructor.data_handle()};

        auto diagnostics = std::vector<Diagnostic>{Diagnostic::createFromInfo(info, creation_context)};

        return ViewManager(diagnostics);
    }


    void print(const ::google::protobuf::RepeatedPtrField<::view::Section> &sections) {
        cerr << "Print Sections" << endl;
        for (const auto &elm : sections) {
            elm.PrintDebugString();
        }
    }

    void print(const ::google::protobuf::RepeatedPtrField< ::view::Diagnostic> &diagnostics) {
        cerr << "Print Diagnostics" << endl;
        cerr << diagnostics.size() << endl;
        for (const auto &elm : diagnostics) {
            print(elm.sections());
        }
        cerr << "End print Diagnostics" << endl;
    }

    void print(::view::ViewResponse* response) {
        cerr << "Print ViewResponse" << endl;
        print(response->diagnostics());
        // print(response->side_notes());
    }

    void ViewManager::getView(::view::ViewResponse* response) {
        std::vector<ptr<::view::Diagnostic>> tmp(ssize(diagnostics));
        transform(diagnostics.begin(), diagnostics.end(), tmp.begin(),
        [](const Diagnostic &diagnostic) {
            return diagnostic.getView();
        });
        for (auto elm : tmp) {
            response->mutable_diagnostics()->AddAllocated(elm);
        }
        print(response);
    }

    void ViewManager::registerInteraction(component_id_t id, InteractionType interaction_type) {
        auto ptr = id_to_interactive_component->find(id);
        if (ptr == id_to_interactive_component->end()) {
            return;
        }
        auto component = ptr->second.lock();
        if (component) {
            component->registerInteraction(interaction_type);
        }
    }

    // ViewServiceImpl

    ViewServiceImpl::ViewServiceImpl(ViewManager vm) : vm(std::move(vm)) {
        ::view::ViewResponse temp;
        vm.getView(&temp);
    }

    ViewServiceImpl ViewServiceImpl::createFromJson(const json &input) {
        return ViewServiceImpl(ViewManager::createFromJson(input));
    }

    ::grpc::Status ViewServiceImpl::GetView(::grpc::ServerContext* context,
                        const ::view::ViewRequest* request,
                        ::view::ViewResponse* response) {
        vm.getView(response);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ViewServiceImpl::Click(::grpc::ServerContext* context,
                        const ::view::ClickRequest* request,
                        ::view::ClickResponse* response) {
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

    ::grpc::Status ViewServiceImpl::CloseSideNote(::grpc::ServerContext* context,
                        const ::view::CloseSideNoteRequest* request,
                        ::view::CloseSideNoteResponse* response) {
        return ::grpc::Status::OK;
    }

    void runViewManagerRPCServer(const json &input) {
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