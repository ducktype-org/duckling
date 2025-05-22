#include "view_manager.hpp"

#include <grpcpp/server_builder.h>
#include <grpcpp/server.h>
#include <grpcpp/grpcpp.h>

namespace dia_app {
namespace view_manager {
    // ViewManager

    ViewManager::ViewManager(std::vector<Diagnostic> diagnostics,
                             std::vector<SidePath> side_paths,
                             std::shared_ptr<id_to_interactive_component_mapping_t> id_to_interactive_component)
                             : diagnostics(std::move(diagnostics)),
                               side_paths(std::move(side_paths)),
                               id_to_interactive_component(std::move(id_to_interactive_component)) {}
    
    ViewManager ViewManager::createFromJson(const json &input) {
        debug("ViewManager::createFromJson begin");
        auto creation_context = CreationContext{
        .id_to_interactive_component=std::make_shared<id_to_interactive_component_mapping_t>(),
        .hl_name_to_id=std::make_unique<std::map<std::string, hl_id_t>>()
        // ,.data_handle = view_constructor.data_handle()
        };
        std::vector<Diagnostic> diagnostics;
        for (uint error_id = 0; error_id < input.size(); ++error_id) {
            ViewConstructor view_constructor(error_id, input);
            diagnostics.emplace_back(Diagnostic::createFromViewConstructor(view_constructor, creation_context));
        }
        debug("ViewManager::createFromJson end");
        return ViewManager(std::move(diagnostics), {}, std::move(creation_context.id_to_interactive_component));
    }

    void ViewManager::getView(::view::ViewResponse* response) {
        debug("ViewManager::getView() begin");
        std::vector<std::unique_ptr<::view::Diagnostic>> diagnostic_view(ssize(diagnostics));
        transform(diagnostics.begin(), diagnostics.end(), diagnostic_view.begin(),
        [](const Diagnostic &diagnostic) {
            return diagnostic.getView();
        });
        for (auto &elm : diagnostic_view) {
            response->mutable_diagnostics()->AddAllocated(elm.release());
        }
        std::vector<std::unique_ptr<::view::SidePath>> side_path_view(ssize(this->side_paths));
        transform(this->side_paths.begin(), this->side_paths.end(), side_path_view.begin(),
        [](const SidePath &side_path) {
            return side_path.getView();
        });
        for (auto &elm : side_path_view) {
            response->mutable_side_paths()->AddAllocated(elm.release());
        }
        debug("ViewManager::getView() end");
    }

    void ViewManager::click(const ::view::ClickRequest* request, ::view::ClickResponse* response) {
        InteractionType interaction_type = [&request](){
            switch(request->click_type()) {
                case ::view::ClickType::CLICK: return InteractionType::Click;
                case ::view::ClickType::CLICK_INTERACTIVE: return InteractionType::ClickInteractive;
                case ::view::ClickType::CLICK_INTERACTIVE_ROLLBACK: return InteractionType::ClickInteractiveRollback;
            }
        }();
        auto id = request->component_id();
        debug("ViewManager::registerInteraction begin");
        debug(id);
        debug(print(interaction_type));
        auto ptr = id_to_interactive_component->find(id);
        if (ptr == id_to_interactive_component->end()) {
            debug("Component with id not found!");
            debug("ViewManager::registerInteraction end");
            response->set_status("Component with given id doesn't exist!");
            return;
        }
        auto component = ptr->second.lock();
        if (component) {
            component->registerInteraction(interaction_type);
        }
        else {
            response->set_status("Component with given id doesn't exist!");
            debug("Component has been deallocated!");
        }
        debug("ViewManager::registerInteraction end");
    }

    void ViewManager::closeSideInfo(const ::view::CloseSideInfoRequest* request, ::view::CloseSideInfoResponse* response) {

    }

    void ViewManager::getEdge(const ::view::EdgeRequest* request, ::view::EdgeResponse* response) {

    }

    // ViewServiceImpl

    ViewServiceImpl::ViewServiceImpl(ViewManager vm) : vm(std::move(vm)) {
        ::view::ViewResponse *temp = new ::view::ViewResponse;
        debug("Test view begin");
        #ifdef DEBUG
        this->vm.getView(temp);
        #endif
        debug("Test view end");
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
        vm.click(request, response);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ViewServiceImpl::CloseSideInfo(::grpc::ServerContext* context,
                        const ::view::CloseSideInfoRequest* request,
                        ::view::CloseSideInfoResponse* response) {
        vm.closeSideInfo(request, response);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ViewServiceImpl::GetEdge(::grpc::ServerContext* context,
                               const ::view::EdgeRequest* request,
                               ::view::EdgeResponse* response) {
        vm.getEdge(request, response);
        return ::grpc::Status::OK;
    }

    void runViewManagerRPCServer(const json &input) {
        ViewServiceImpl service = ViewServiceImpl::createFromJson(input);
        grpc::ServerBuilder builder;
        builder.AddListeningPort("localhost:50051", grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cerr << "ViewManager started on port 50051\n";
        server->Wait();
    }
}
}