#pragma once
#include "diagnostics.hpp"
#include "side_panel.hpp"

#include <json/json.hpp>
#include <proto/view.grpc.pb.h>
#include <proto/view.pb.h>

namespace dia_app {
	namespace view_manager {

		using nlohmann::json;

		class ViewManager {
		private:
			std::vector<Diagnostic>                                diagnostics;
			std::vector<SidePath>                                  side_paths;
			std::vector<std::shared_ptr<ViewConstructor>> view_constructors;
            std::shared_ptr<CreationContext> creation_context;

		public:
			ViewManager(
				std::vector<Diagnostic>                                diagnostics,
				std::vector<SidePath>                                  side_paths,
				std::vector<std::shared_ptr<ViewConstructor>> view_constructors,
                std::shared_ptr<CreationContext> creation_context
			);

			static ViewManager createFromJson(const json& input);

			void getView(::view::ViewResponse* response);

			void click(const ::view::ClickRequest* request, ::view::ClickResponse* response);

			void closeSideInfo(
				const ::view::CloseSideInfoRequest* request, ::view::CloseSideInfoResponse* response
			);

			void getEdge(const ::view::EdgeRequest* request, ::view::EdgeResponse* response);
		};

		class ViewServiceImpl: public ::view::ViewService::Service {
		private:
			ViewManager vm;

		public:
			ViewServiceImpl(ViewManager vm);

			static ViewServiceImpl createFromJson(const json& input);

			::grpc::Status GetView(
				::grpc::ServerContext*     context,
				const ::view::ViewRequest* request,
				::view::ViewResponse*      response
			) override;

			::grpc::Status Click(
				::grpc::ServerContext*      context,
				const ::view::ClickRequest* request,
				::view::ClickResponse*      response
			) override;

			::grpc::Status CloseSideInfo(
				::grpc::ServerContext*              context,
				const ::view::CloseSideInfoRequest* request,
				::view::CloseSideInfoResponse*      response
			) override;

			::grpc::Status GetEdge(
				::grpc::ServerContext*     context,
				const ::view::EdgeRequest* request,
				::view::EdgeResponse*      response
			) override;
		};

		void runViewManagerRPCServer(const json& input);
	}  // namespace view_manager
}  // namespace dia_app
