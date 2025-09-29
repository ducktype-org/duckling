#include "view_manager.hpp"

#include "fetcher.hpp"

namespace dia_app {
	namespace view_manager {
		// ViewManager

		ViewManager::ViewManager(std::vector<std::pair<Diagnostic, ViewConstructor>> diagnostics):
			  diagnostics(std::move(diagnostics)) {}

		ViewManager ViewManager::createFromJson(json input) {
			fetcher::initialize(std::move(input));

			hl_id_t              next_component_id = 0;
			std::function<u32()> get_next_id
				= [&next_component_id]() { return next_component_id++; };
			base::HashMap<std::string, hl_id_t>               group_to_id_map;
			hl_id_t                                           next_group_id = 0;
			std::function<view_manager::hl_id_t(std::string)> group_to_id
				= [&group_to_id_map, &next_group_id](const std::string& str) {
					  auto ptr = group_to_id_map.find(str);
					  if (ptr == group_to_id_map.end()) {
						  group_to_id_map.put(str, next_group_id);
						  next_group_id++;
					  }
					  return group_to_id_map[str];
				  };
			base::HashMap<std::string, hl_id_t>               hl_name_to_id_map;
			hl_id_t                                           next_hl_id = 0;
			std::function<view_manager::hl_id_t(std::string)> hl_name_to_id
				= [&hl_name_to_id_map, &next_hl_id](const std::string& str) {
					  auto ptr = hl_name_to_id_map.find(str);
					  if (ptr == hl_name_to_id_map.end()) {
						  hl_name_to_id_map.put(str, next_hl_id);
						  next_hl_id++;
					  }
					  return hl_name_to_id_map[str];
				  };

			std::vector<std::pair<Diagnostic, ViewConstructor>> diagnostics;
			for (uint info_group_id = 0; info_group_id < fetcher::getInfoGroupCount();
			     ++info_group_id) {
				auto vc = fetcher::generateViewConstructor(info_group_id);

				diagnostics.emplace_back(
					Diagnostic::createFromViewConstructor(
						vc, hl_name_to_id, get_next_id, group_to_id
					),
					vc
				);
			}
			return ViewManager(std::move(diagnostics));
		}

		void ViewManager::getView(::view::ViewResponse* response) {
			for (auto& [diagnostic, vc]: this->diagnostics)
				response->mutable_diagnostics()->AddAllocated(diagnostic.getView(vc).release());
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
