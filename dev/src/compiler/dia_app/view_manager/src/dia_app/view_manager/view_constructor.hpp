#pragma once
#include "template_application.hpp"
#include "dia_parser.hpp"

namespace dia_app {

	struct ViewConstructor {
	private:
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
	
	public:
		ViewConstructor(u32 info_group_no, const json& data);

		// ViewManager should use only `getSecondaryInfo` and `getMainInfo`.
		message_template::Info getSecondaryInfo(InfoID id);
		message_template::Info getMainInfo();

	private:
		const Entity &getEntity(EntityID id);

		DisplayPtr getLazyElement(LazyDisplayID id);

		std::set<InfoID> assocInfosFromEntity(EntityID id);

		message_template::Info getInfo(const InfoParams& param_data);

		void fetchInfo(InfoID id);
		void fetchEntity(EntityID id);
		void fetchLazyElement(LazyDisplayID id);

		friend class dia_file::EntityDElement;
		friend class dia_file::LazyDElement;
	};
}
