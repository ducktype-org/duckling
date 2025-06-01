#include "view_manager.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

namespace dia_app {
	namespace view_manager {
		// ViewManager

		ViewManager::ViewManager(
			std::vector<Diagnostic>                                diagnostics,
			std::vector<SidePath>                                  side_paths,
				std::vector<std::shared_ptr<ViewConstructor>> view_constructors,
            std::shared_ptr<CreationContext> creation_context
		):
			  diagnostics(std::move(diagnostics)),
			  side_paths(std::move(side_paths)),
			  view_constructors(std::move(view_constructors)),
              creation_context(std::move(creation_context)) {}

		ViewManager ViewManager::createFromJson(const json& input) {
			debug("ViewManager::createFromJson begin");
			auto creation_context =
            std::make_shared<CreationContext>(
                std::make_shared<id_to_interactive_component_mapping_t>(),
				std::make_shared<id_to_view_constructor_mapping_t>(),
				std::make_unique<std::map<std::string, hl_id_t>>(),
                std::optional<DataHandle>{},
                std::weak_ptr<ViewConstructor>{}
            );
			std::vector<Diagnostic>                       diagnostics;
			std::vector<std::shared_ptr<ViewConstructor>> view_constructors;
			for (uint error_id = 0; error_id < input.size(); ++error_id) {
				auto view_constructor = std::make_shared<ViewConstructor>(error_id, input);
				// Update the view constructor context (necessary for fetching lazy content).
				creation_context->data_handle.emplace(view_constructor->dataHandle());

				view_constructors.emplace_back(view_constructor);
				creation_context->view_constructor = view_constructor;
				diagnostics.emplace_back(
					Diagnostic::createFromViewConstructor(view_constructor, creation_context)
				);
			}
			debug("ViewManager::createFromJson end");
			auto result = ViewManager(
				std::move(diagnostics),
				std::vector<SidePath>(),
				std::move(view_constructors),
				std::move(creation_context)
			);
			return result;
		}

		void ViewManager::getView(::view::ViewResponse* response) {
			debug("ViewManager::getView() begin");
			std::vector<std::unique_ptr<::view::Diagnostic>> diagnostic_view(ssize(diagnostics));
			transform(
				diagnostics.begin(),
				diagnostics.end(),
				diagnostic_view.begin(),
				[](const Diagnostic& diagnostic) { return diagnostic.getView(); }
			);
			for (auto& elm: diagnostic_view)
				response->mutable_diagnostics()->AddAllocated(elm.release());
			std::vector<std::unique_ptr<::view::SidePath>> side_path_view(ssize(this->side_paths));
			transform(
				this->side_paths.begin(),
				this->side_paths.end(),
				side_path_view.begin(),
				[](const SidePath& side_path) { return side_path.getView(); }
			);
			for (auto& elm: side_path_view)
				response->mutable_side_paths()->AddAllocated(elm.release());
			debug("ViewManager::getView() end");
		}

		void ViewManager::click(
			const ::view::ClickRequest* request, ::view::ClickResponse* response
		) {
			InteractionType interaction_type = [&request]() {
				switch (request->click_type()) {
				case ::view::ClickType::CLICK:
					return InteractionType::Click;
				case ::view::ClickType::CLICK_INTERACTIVE:
					return InteractionType::ClickInteractive;
				case ::view::ClickType::CLICK_INTERACTIVE_ROLLBACK:
					return InteractionType::ClickInteractiveRollback;
				}
			}();
			auto id = request->component_id();
			debug("ViewManager::registerInteraction begin");
			debug(id);
			debug(print(interaction_type));
			auto ptr = creation_context->id_to_interactive_component->find(id);
			if (ptr == creation_context->id_to_interactive_component->end()) {
				debug("Component with id not found!");
				debug("ViewManager::registerInteraction end");
				response->set_status("Component with given id doesn't exist!");
				return;
			}
			auto component = ptr->second.lock();
			if (component) {
				std::function<void(std::vector<SidePath>)> after_open = [this](std::vector<SidePath> paths) {
					this->side_paths.insert(this->side_paths.begin(), std::make_move_iterator(paths.begin()), std::make_move_iterator(paths.end()));
				};
				auto interaction_context = InteractionContext{
					.after_open = after_open,
					.creation_context = this->creation_context
				};
				component->registerInteraction(interaction_type, interaction_context);
				response->set_status("Click registered successfully!");
			} else {
				response->set_status("Component with given id doesn't exist!");
				debug("Component has been deallocated!");
			}
			debug("ViewManager::registerInteraction end");
		}

		void ViewManager::closeSideInfo(
			const ::view::CloseSideInfoRequest* request, ::view::CloseSideInfoResponse* response
		) {
			side_info_id_t side_info_id = request->side_info_id();
			for (auto &path : this->side_paths) {
				path.tryCloseSideInfo(side_info_id);
			}
			this->side_paths.erase(std::remove_if(this->side_paths.begin(), this->side_paths.end(),
			[](SidePath &path) {
				return path.isEmpty();
			}), this->side_paths.end());
		}

		void ViewManager::getEdge(
			const ::view::EdgeRequest* request, ::view::EdgeResponse* response
		) {
			side_info_id_t side_info_id = request->side_info_id();
			edge_id_t edge_id = request->edge_id();
			for (auto &path : this->side_paths) {
				path.tryGetEdge(side_info_id, edge_id, this->creation_context);
			}
		}

		// ViewServiceImpl

		ViewServiceImpl::ViewServiceImpl(ViewManager vm): vm(std::move(vm)) {
			::view::ViewResponse* temp = new ::view::ViewResponse;
			debug("Test view begin");
#ifdef DEBUG
			this->vm.getView(temp);
#endif
			debug("Test view end");
		}

		ViewServiceImpl ViewServiceImpl::createFromJson(const json& input) {
			return ViewServiceImpl(ViewManager::createFromJson(input));
		}

		::grpc::Status ViewServiceImpl::GetView(
			::grpc::ServerContext*     context,
			const ::view::ViewRequest* request,
			::view::ViewResponse*      response
		) {
			vm.getView(response);
			return ::grpc::Status::OK;
		}

		::grpc::Status ViewServiceImpl::Click(
			::grpc::ServerContext*      context,
			const ::view::ClickRequest* request,
			::view::ClickResponse*      response
		) {
			vm.click(request, response);
			return ::grpc::Status::OK;
		}

		::grpc::Status ViewServiceImpl::CloseSideInfo(
			::grpc::ServerContext*              context,
			const ::view::CloseSideInfoRequest* request,
			::view::CloseSideInfoResponse*      response
		) {
			vm.closeSideInfo(request, response);
			return ::grpc::Status::OK;
		}

		::grpc::Status ViewServiceImpl::GetEdge(
			::grpc::ServerContext*     context,
			const ::view::EdgeRequest* request,
			::view::EdgeResponse*      response
		) {
			vm.getEdge(request, response);
			return ::grpc::Status::OK;
		}

		void runViewManagerRPCServer(const json& input) {
			ViewServiceImpl     service = ViewServiceImpl::createFromJson(input);
			grpc::ServerBuilder builder;
			builder.AddListeningPort("localhost:50051", grpc::InsecureServerCredentials());
			builder.RegisterService(&service);
			std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
			std::cerr << "ViewManager started on port 50051\n";
			server->Wait();
		}
	}
}
