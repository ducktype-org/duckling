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
			/**
			 * @brief Construct a ViewManager with a list of diagnostics and their constructors.
			 */
			ViewManager(std::vector<std::pair<Diagnostic, ViewConstructor>> diagnostics);

			/**
			 * @brief Build a ViewManager from the full diagnostic file in JSON.
			 *
			 * Parses the diagnostic groups, initializes ViewConstructors and the
			 * corresponding Diagnostic structures.
			 */
			static ViewManager createFromJson(json input);

			/**
			 * @brief Serialize the current view (all diagnostics) to gRPC response.
			 */
			void getView(::view::ViewResponse* response);

			/**
			 * @brief Apply a click interaction on a component and return updated view.
			 */
			void click(const ::view::ClickRequest* request, ::view::ClickResponse* response);

			/**
			 * @brief Attempt to close a side info from the current side path.
			 */
			void closeSideInfo(
				const ::view::CloseSideInfoRequest* request, ::view::CloseSideInfoResponse* response
			);

			/**
			 * @brief Resolve and follow an explore edge in the side panel.
			 */
			void getEdge(const ::view::EdgeRequest* request, ::view::EdgeResponse* response);
		};

		/**
		 * @brief gRPC service implementation delegating to ViewManager.
		 */
		class ViewServiceImpl: public ::view::ViewService::Service {
		private:
			ViewManager vm;

		public:
			/**
			 * @brief Construct the service with a ready ViewManager.
			 */
			ViewServiceImpl(ViewManager vm);

			/**
			 * @brief Build the service from diagnostic JSON.
			 */
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

		/**
		 * @brief Run a gRPC server exposing the ViewService over RPC.
		 *
		 * Creates a service from the provided diagnostic JSON and starts the
		 * server loop on an available port.
		 */
		void runViewManagerRPCServer(const json& input);
	}  // namespace view_manager
}  // namespace dia_app
