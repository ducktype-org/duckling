#include "fetcher.hpp"

#include <base/maps.hpp>

namespace dia_app {
	namespace fetcher {

		std::vector<json> info_groups;

		base::HashMap<InfoID, json>        infos;
		base::HashMap<EntityID, json>      entities;
		base::HashMap<LazyDisplayID, json> elements;

		void initialize(json&& diagnostic_file) {
			if (!diagnostic_file.is_array()) {
				// Parse the diagnostic file format with a laziness mock enabled.
				ASSUME_HAS(diagnostic_file, "content");

				// Store lazy infos (do not evaluate).
				if (diagnostic_file.contains("lazy_infos"))
					infos = json_to_map<json>(diagnostic_file["lazy_infos"]);
				// Store lazy entities (do not evaluate).
				if (diagnostic_file.contains("lazy_entities"))
					entities = json_to_map<json>(diagnostic_file["lazy_entities"]);
				// Store lazy elements (do not evaluate).
				if (diagnostic_file.contains("lazy_elements"))
					elements = json_to_map<json>(diagnostic_file["lazy_elements"]);

				// Set diagnostic file to the actual file contents.
				diagnostic_file = diagnostic_file["content"];
				CORE_ASSERT(
					diagnostic_file.is_array(), "Diagnostic file contents must be an array."
				);
			}
			// Convert the diagnostic file contents into a vector.
			info_groups = diagnostic_file;
		}

		u32 getInfoGroupCount() { return info_groups.size(); }

		ViewConstructor generateViewConstructor(u32 idx) {
			CORE_ASSERT(idx < info_groups.size(), "View constructor index out of range.");
			return ViewConstructor(info_groups[idx]);
		}

		FetchError::FetchError(std::string message): message(std::move(message)) {}

		const char* FetchError::what() const noexcept { return message.data(); }

		dia_file::InfoParams fetchInfo(InfoID id) {
			if (infos.contains(id)) return infos.at(id);
			throw FetchError("Tried to fetch unavailable info.");
		}

		dia_file::Entity fetchEntity(EntityID id) {
			if (entities.contains(id)) return entities.at(id);
			throw FetchError("Tried to fetch unavailable entity.");
		}

		dia_file::DisplayPtr fetchLazyElement(LazyDisplayID id) {
			if (elements.contains(id)) return dia_file::parse(elements.at(id));
			throw FetchError("Tried to fetch unavailable lazy element.");
		}

	}
}
