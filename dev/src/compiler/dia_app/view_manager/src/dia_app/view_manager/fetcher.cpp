#include "fetcher.hpp"

#include <base/maps.hpp>

namespace dia_app {
	namespace fetcher {

		std::vector<json> info_pointer_messages;

		base::HashMap<InfoID, json>        infos;
		base::HashMap<EntityID, json>      entities;
		base::HashMap<LazyDisplayID, json> elements;

		void initialize(const json& diagnostic_file) {
			CORE_ASSERT(
				diagnostic_file.is_array(), "Diagnostic file contents must be an array."
			);
			// Convert the diagnostic file contents into a vector.
			info_pointer_messages = diagnostic_file;
		}

		u32 getInfoGroupCount() { return info_pointer_messages.size(); }

		ViewConstructor generateViewConstructor(u32 idx) {
			CORE_ASSERT(idx < info_pointer_messages.size(), "View constructor index out of range.");
			return ViewConstructor(info_pointer_messages[idx]);
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
