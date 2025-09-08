#include "view_constructor.hpp"

#include "fetcher.hpp"

namespace dia_app {

	ViewConstructor::ViewConstructor(const json& info_group) {
		// Parse the main info parameters.
		main_info = InfoParams(info_group["main_info"]);

		// Parse the collection of secondary info parameters.
		ASSUME_HAS(info_group, "secondary_infos");
		ASSUME_OBJ(info_group["secondary_infos"]);
		for (auto& [id, info]: info_group["secondary_infos"].items())
			secondary_infos.put(id, InfoParams(info));

		// Parse the optional list of displayed secondary infos.
		if (info_group.contains("displayed_secondary_infos")) {
			ASSUME_ARR(info_group, "displayed_secondary_infos");
			for (auto& el: info_group["displayed_secondary_infos"])
				displayed_secondary_infos.push_back(el);
		}

		// Parse the collection of entities.
		ASSUME_HAS(info_group, "entities");
		entities = json_to_map<Entity>(info_group["entities"]);
	}

	message_template::Info ViewConstructor::getSecondaryInfo(InfoID id) {
		// Fetch data if needed.
		if (!secondary_infos.contains(id)) fetchInfo(id);
		CORE_ASSERT(secondary_infos.contains(id), "failed to fetch secondary info data");
		return getInfo(secondary_infos[id]);
	}

	message_template::Info ViewConstructor::getMainInfo() { return getInfo(main_info); }

	const ViewConstructor::Entity& ViewConstructor::getEntity(EntityID id) {
		// Fetch data if needed.
		if (!entities.contains(id)) fetchEntity(id);
		CORE_ASSERT(entities.contains(id), "failed to fetch entity");
		return entities[id];
	}

	ViewConstructor::DisplayPtr ViewConstructor::getLazyElement(LazyDisplayID id) {
		// Fetch data if needed.
		if (!fetched_components.contains(id)) fetchLazyElement(id);
		CORE_ASSERT(fetched_components.contains(id), "failed to fetch lazy element contents");
		return fetched_components[id];
	}

	std::set<InfoID> ViewConstructor::assocInfosFromEntity(EntityID id) {
		const auto& entity = getEntity(id);
		// Scan the entity metadata for associated infos.
		auto res = std::set<InfoID>(entity.assoc_infos.begin(), entity.assoc_infos.end());

		// TODO: more functionalities may be added here.

		return res;
	}

	message_template::Info ViewConstructor::getInfo(const InfoParams& param_data) {
		return message_template::apply(*this, param_data);
	}

	void ViewConstructor::fetchInfo(InfoID id) {
		InfoParams info = fetcher::fetchInfo(id);
		secondary_infos.put(id, info);
	}

	void ViewConstructor::fetchEntity(EntityID id) {
		Entity entity = fetcher::fetchEntity(id);
		entities.put(id, entity);
	}

	void ViewConstructor::fetchLazyElement(LazyDisplayID id) {
		DisplayPtr element = fetcher::fetchLazyElement(id);
		fetched_components.put(id, element);
	}
}
