#pragma once
#include "dia_parser.hpp"
#include "template_application.hpp"

namespace dia_app {

	/**
	 * @brief An object responsible for one info group from the diagnostic
	 * file.
	 *
	 * Acts as a proxy for fetching the data related to this info group
	 * from the compiler.
	 *
	 * After being constructed with the diagnostic file data, can be queried
	 * for its member infos which are lazily evaluated from info templates
	 * on demand.
	 *
	 * Implements the laziness mechanisms for lazy display elements,
	 * lazy entities, and lazy infos.
	 */
	struct ViewConstructor {
	private:
		using InfoParams = dia_file::InfoParams;
		using Entity     = dia_file::Entity;
		using DisplayPtr = dia_file::DisplayPtr;

		// The main info (has to be of type error or warning).
		InfoParams main_info;
		// The collection of secondary infos which can appear in the info group.
		base::HashMap<InfoID, InfoParams> secondary_infos;
		// The collection of entities shared across all infos in the info group.
		base::HashMap<EntityID, Entity> entities;
		// The list of ids of secondary infos to be initially displayed
		// alongside the main info in the specified order
		// (others can be displayed after interactions).
		std::vector<InfoID> displayed_secondary_infos;

		// The collection of component subtrees which have been fetched
		// since reading the diagnostic file.
		// Since each component tree is immutable, instead of updating
		// subtrees in-place upon fetching, we store their them in
		// this collection.
		base::HashMap<LazyDisplayID, DisplayPtr> fetched_components;

	public:
		/**
		 * @brief Construct a new View Constructor for an info group.
		 *
		 * @param info_group The info group data in JSON format.
		 */
		ViewConstructor(const json& info_group);

		// Get the main info of this info group after applying the info
		// template.
		message_template::Info getMainInfo();
		// Get the secondary info identified by `id` after applying the
		// appropriate info template.
		// Fetches and caches the info parameters if necessary.
		message_template::Info getSecondaryInfo(InfoID id);

	private:
		// Get the entity identified by `id`, fetching and caching the result
		// if necessary.
		const Entity& getEntity(EntityID id);

		// Get the lazy element identified by `id`, fetching and caching
		// the result if necessary.
		DisplayPtr getLazyElement(LazyDisplayID id);

		// Evaluate the info template from `param_data`.
		message_template::Info getInfo(const InfoParams& param_data);

		// Get the list of infos associated with the entity identified by `id`.
		// Fetches and caches the entity if necessary.
		std::set<InfoID> assocInfosFromEntity(EntityID id);

		// Fetch the parametrization of the info identified by `id` and cache
		// the result.
		void fetchInfo(InfoID id);
		// Fetch the entity identified by `id` and cache the result.
		void fetchEntity(EntityID id);
		// Fetch the lazy element identified by `id` and cache the result.
		void fetchLazyElement(LazyDisplayID id);

		// Entity- and Lazy- display elements use the otherwise private
		// methods: `getLazyElement` and `assocInfosFromEntity`,
		// so we make them friends.
		friend class dia_file::EntityDElement;
		friend class dia_file::LazyDElement;
	};
}
