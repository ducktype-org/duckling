// #include "side_panel.hpp"

// #include <utility>

// namespace dia_app {
// 	namespace view_manager {
// 		// SideEdge
// 		SideEdge::SideEdge(edge_id_t edge_id, std::string description):
// 			  edge_id(edge_id),
// 			  description(std::move(description)) {}

// 		std::unique_ptr<::view::SideEdge> SideEdge::getView() const {
// 			auto result = std::make_unique<::view::SideEdge>();
// 			result->set_edge_id(this->edge_id);
// 			result->set_description(this->description);
// 			return result;
// 		}

// 		// SideInfo
// 		SideInfo::SideInfo(
// 			Metadata                              metadata,
// 			std::shared_ptr<Component>            header,
// 			std::vector<std::unique_ptr<Section>> sections,
// 			std::vector<SideEdge>                 edges
// 		):
// 			  id(getNewId()),
// 			  metadata(metadata),
// 			  header(std::move(header)),
// 			  sections(std::move(sections)),
// 			  edges(std::move(edges)) {}

// 		SideInfo SideInfo::createFromInfoID(
// 			InfoID                        info_handle,
// 			std::shared_ptr<ViewConstructor>& view_constructor,
// 			std::shared_ptr<CreationContext>                  creation_context
// 		) {
// 			auto info     = *view_constructor->loadSecondaryInfo(info_handle);
// 			auto metadata = Metadata::createFromInfo(info);
// 			std::vector<std::unique_ptr<Section>> sections;
// 			assert(info.header_message);
// 			auto header       = info.header_message->toComponent(creation_context);
// 			auto code_section = CodeSection::createFromInfo(info, creation_context);
// 			if (code_section) sections.emplace_back(std::move(code_section));
// 			if (info.description) {
// 				sections.emplace_back(
// 					std::make_unique<TextSection>(info.description->toComponent(creation_context))
// 				);
// 			}
// 			debug("Number of sections: ", ssize(sections));
// 			std::vector<SideEdge> edges;
// 			for (const auto& edge: info.explore_edges) {
// 				edges.emplace_back(
// 					edge.handle, edge.description
// 				);
// 			}
// 			return SideInfo(metadata, std::move(header), std::move(sections), std::move(edges));
// 		}

// 		std::unique_ptr<::view::SideInfo> SideInfo::getView() const {
// 			auto result = std::make_unique<::view::SideInfo>();
// 			result->set_side_info_id(this->id);
// 			result->set_allocated_metadata(this->metadata.getView().release());
// 			result->set_allocated_header(
// 				concatNoHlLines(filterEmptyLines(this->header->getNoHlView())).release()
// 			);
// 			{
// 				for (const auto& section: this->sections) {
// 					auto section_view = section->getView();
// 					result->mutable_sections()->AddAllocated(section_view.release());
// 				}
// 			}
// 			{
// 				for (const auto& edge: this->edges) {
// 					auto edge_view = edge.getView();
// 					result->mutable_edges()->AddAllocated(edge_view.release());
// 				}
// 			}
// 			return result;
// 		}

// 		side_info_id_t SideInfo::getId() { return this->id; }

// 		base::Optional<SideInfo> SideInfo::getEdge(edge_id_t handle, std::shared_ptr<ViewConstructor> view_constructor, std::shared_ptr<CreationContext> creation_context) {
// 			for (const auto &edge : this->edges) {
// 				if (edge.edge_id == handle) {
// 					return SideInfo::createFromInfoID(handle, view_constructor, creation_context);
// 				}
// 			}
// 			return {};
// 		}

// 		// SidePath
// 		SidePath::SidePath(std::vector<SideInfo> infos,
// 							std::weak_ptr<ViewConstructor> view_constructor):
// 							infos(std::move(infos)),
// 							view_constructor(std::move(view_constructor)) {}

// 		SidePath SidePath::createFromInfoID(
// 			InfoID                        info_handle,
// 			std::shared_ptr<ViewConstructor>& view_constructor,
// 			std::shared_ptr<CreationContext>                  creation_context
// 		) {
// 			std::vector<SideInfo> infos;
// 			auto                  info
// 				= SideInfo::createFromInfoID(info_handle, view_constructor, creation_context);
// 			infos.emplace_back(std::move(info));
// 			return SidePath(std::move(infos), view_constructor);
// 		}

// 		std::unique_ptr<::view::SidePath> SidePath::getView() const {
// 			auto result = std::make_unique<::view::SidePath>();
// 			for (const auto& info: this->infos)
// 				result->mutable_infos()->AddAllocated(info.getView().release());
// 			return result;
// 		}

// 		bool SidePath::isEmpty() { return this->infos.empty(); }

// 		void SidePath::tryCloseSideInfo(side_info_id_t side_info_id) {
// 			auto iter = std::find_if(this->infos.begin(), this->infos.end(),
// 			[side_info_id](SideInfo &info) {
// 				return info.getId() == side_info_id;
// 			});
// 			this->infos.erase(iter, this->infos.end());
// 		}

// 		void SidePath::tryGetEdge(side_info_id_t side_info_id, edge_id_t edge_id, std::shared_ptr<CreationContext> creation_context) {
// 			auto iter = std::find_if(this->infos.begin(), this->infos.end(),
// 			[side_info_id](SideInfo &info) {
// 				return info.getId() == side_info_id;
// 			});
// 			if (iter == this->infos.end()) {
// 				return;
// 			}
// 			this->infos.erase(iter + 1, this->infos.end());
// 			auto info = iter->getEdge(edge_id, this->view_constructor.lock(), std::move(creation_context));
// 			if (info.has_value()) {
// 				this->infos.emplace_back(std::move(info.value()));
// 			}
// 		}

// 		// Custom

// 		void openSideEntries(
// 			const std::vector<side_entry_id_t>& side_entries, const std::weak_ptr<ViewConstructor> &view_constructor, InteractionContext& interaction_context
// 		) {
// 			auto vc = view_constructor.lock();
//             std::vector<SidePath> paths;
//             for (const auto &side_entry: side_entries) {
//                 paths.emplace_back(SidePath::createFromInfoID(side_entry, vc, interaction_context.creation_context));
//             }
//             if (interaction_context.after_open.has_value()) {
//                 interaction_context.after_open.value()(std::move(paths));
//             }
// 		}
// 	}  // namespace view_manager
// }  // namespace dia_app
