#pragma once
#include "components.hpp"
#include "diagnostics.hpp"

namespace dia_app {
	namespace view_manager {
		using edge_id_t      = uint32_t;
		using side_info_id_t = uint32_t;

		class SideEdge {
		public:
			edge_id_t   edge_id;
			std::string description;

			SideEdge(edge_id_t edge_id, std::string description);

			std::unique_ptr<::view::SideEdge> getView() const;
		};

		class SideInfo {
		private:
			side_info_id_t                        id;
			Metadata                              metadata;
			std::shared_ptr<Component>            header;
			std::vector<std::unique_ptr<Section>> sections;
			std::vector<SideEdge>                 edges;

		public:
			SideInfo(
				Metadata                              metadata,
				std::shared_ptr<Component>            header,
				std::vector<std::unique_ptr<Section>> sections,
				std::vector<SideEdge>                 edges
			);

			static SideInfo createFromInfoID(
				InfoID info_handle, std::shared_ptr<ViewConstructor>& view_constructor
			);

			std::unique_ptr<::view::SideInfo> getView() const;

			side_info_id_t getId();

			base::Optional<SideInfo> getEdge(
				edge_id_t handle, std::shared_ptr<ViewConstructor> view_constructor
			);
		};

		class SidePath {
		private:
			std::vector<SideInfo>          infos;
			std::weak_ptr<ViewConstructor> view_constructor;

		public:
			SidePath(std::vector<SideInfo> infos, std::weak_ptr<ViewConstructor> view_constructor);

			static SidePath createFromInfoID(
				InfoID info_handle, std::shared_ptr<ViewConstructor>& view_constructor
			);

			std::unique_ptr<::view::SidePath> getView() const;

			bool isEmpty();

			void tryCloseSideInfo(side_info_id_t side_info_id);

			void tryGetEdge(side_info_id_t side_info_id, edge_id_t edge_id);
		};
	}  // namespace view_manager
}  // namespace dia_app
