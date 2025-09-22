#include "view_manager.hpp"

namespace dia_app {
	namespace view_manager {
		// ViewManager

		ViewManager::ViewManager(std::vector<Diagnostic> diagnostics):
			  diagnostics(std::move(diagnostics)) {}

		ViewManager ViewManager::createFromJson(const json& input) {
			UNIMPLEMENTED();
			std::vector<Diagnostic> diagnostics;
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
			for (const auto& diagnostic: this->diagnostics)
				response->mutable_diagnostics()->AddAllocated(diagnostic.getView().release());
		}

		void ViewManager::click(
			const ::view::ClickRequest* request, ::view::ClickResponse* response
		) {
			UNIMPLEMENTED();
		}

		void ViewManager::closeSideInfo(
			const ::view::CloseSideInfoRequest* request, ::view::CloseSideInfoResponse* response
		) {
			UNIMPLEMENTED();
		}

		void ViewManager::getEdge(
			const ::view::EdgeRequest* request, ::view::EdgeResponse* response
		) {
			UNIMPLEMENTED();
		}

		// ViewServiceImpl

		ViewServiceImpl::ViewServiceImpl(ViewManager vm): vm(std::move(vm)) {}

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

		void runViewManagerRPCServer(const json& input) { UNIMPLEMENTED(); }
	}
}
