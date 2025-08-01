#pragma once
#include "template_application.hpp"
#include "dia_parser.hpp"

namespace dia_app {

	struct ViewConstructor {
		using InfoParams = dia_file::InfoParams;
		using Entity = dia_file::Entity;
		using DisplayPtr = dia_file::DisplayPtr;

		// Main info section (error or warning).
		InfoParams main_info;
		// Collection of secondary infos which can appear in the message.
		base::HashMap<InfoID, InfoParams> secondary_infos;
		base::HashMap<EntityID, Entity> entities;
		// List of secondary infos displayed alongside the main info
		// in the specified order (others can be displayed after
		// interactions).
		std::vector<InfoID>     displayed_secondary_infos;

		// Collection of component subtrees which have been fetched since reading
		// the diagnostic file (since each component tree is immutable,
		// instead of updating subtrees in-place upon fetching, we store
		// their index in this collection).
		base::HashMap<LazyDisplayID, DisplayPtr> fetched_components;

		ViewConstructor(uint info_group_no, const json& data) {
			// Access the requested info group data.
			const json& info_group = data[info_group_no];

			// Parse the main info parameters.
			main_info = InfoParams(info_group["main_info"]);

			// Parse the collection of secondary info parameters.
			ASSUME_HAS(info_group, "secondary_infos");
			ASSUME_OBJ(info_group["secondary_infos"]);
			for (auto &[id, info]: info_group["secondary_infos"].items()) {
				secondary_infos.put(id, InfoParams(info));
			}

			// Parse the optional list of displayed secondary infos.
			if (info_group.contains("displayed_secondary_infos")) {
				ASSUME_ARR(info_group, "displayed_secondary_infos");
				for (auto& el: info_group["displayed_secondary_infos"])
					displayed_secondary_infos.push_back(el);
			}

			// Parse the collection of entities.
			ASSUME_HAS(info_group, "entities");
			ASSUME_OBJ(info_group["entities"]);
			for (auto &[id, e] : info_group["entities"].items()) {
				entities.put(id, Entity(e));
			}
		}

		// ViewManager should use only `getSecondaryInfo` and `getMainInfo`.
		message_template::Info getSecondaryInfo(InfoID id) {
			// Fetch data if needed.
			if (!secondary_infos.contains(id)) {
				fetchInfo(id);
			}
			ASSUME(secondary_infos.contains(id), "failed to fetch secondary info data");
			return getInfo(secondary_infos[id]);
		}

		message_template::Info getMainInfo() {
			return getInfo(main_info);
		}

		const Entity &getEntity(EntityID id) {
			// Fetch data if needed.
			if (!entities.contains(id)) {
				fetchEntity(id);
			}
			ASSUME(entities.contains(id), "failed to fetch entity");
			return entities[id];
		}

		DisplayPtr getLazyElement(LazyDisplayID id) {
			// Fetch data if needed.
			if (!fetched_components.contains(id)) {
				fetchLazyElement(id);
			}
			ASSUME(fetched_components.contains(id), "failed to fetch lazy element contents");
			return fetched_components[id];
		}

		std::set<InfoID> assocInfosFromEntity(EntityID id) {
			const auto &entity = getEntity(id);
			// Scan the entity metadata for associated infos.
			auto res = std::set<InfoID>(entity.assoc_infos.begin(), entity.assoc_infos.end());
			
			// TODO: more functionalities may be added here.
			
			return res;
		}

	private:
		message_template::Info getInfo(const InfoParams& param_data) {
			return message_template::apply(*this, param_data);
		}

		void fetchInfo(InfoID id) {
			// @TODO
		}

		void fetchEntity(EntityID id) {
			// @TODO
		}

		void fetchLazyElement(LazyDisplayID id) {
			// @TODO
		}
	};
}
