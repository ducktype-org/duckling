#pragma once
#include "diagnostics.hpp"

#include <proto/view.grpc.pb.h>
#include <proto/view.pb.h>

#include <json/json.hpp>

namespace dia_app {
	namespace view_manager {

		using nlohmann::json;

		/**
		 * `ViewManager` holds the state of one client.
		 * It contains multiple `ViewConstructors`, as a single `ViewConstructor` is responsible for
		 * one `Diagnostic`.
		 * The `ViewManager` has to handle interactions with every `Component` inside it,
		 * so it has to maintain a mapping from `componentId` to `Component`s (and similar data,
		 * such as what `ViewConstructor` created such `Component`).
		 *
		 * The UI sends only that an interaction happened with some component/side_info
		 * and what type of interaction it was, so I don't have to care much about
		 * reusing id's. I can just assure that id's are unique globally.
		 *
		 * I'm NOT going to free memory from `Components` that are not visible,
		 * but were visible before - just reset them to initial state.
		 */
		class ViewManager {
		private:
			std::vector<std::pair<Diagnostic, ViewConstructor>> diagnostics;

		public:
			ViewManager(std::vector<std::pair<Diagnostic, ViewConstructor>> diagnostics);

			static ViewManager createFromJson(json input);

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
