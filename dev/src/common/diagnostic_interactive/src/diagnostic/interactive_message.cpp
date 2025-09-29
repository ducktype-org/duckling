#include "interactive_message.hpp"

#include "interactive_logger.hpp"

#include <typesystem/higher/abstract_type.hpp>

#include "base/string_id.hpp"

using nlohmann::json;

json dia::InteractiveMessage::tojson() const {
	const SerializationParams& params = InteractiveLogger::params();
	json                       res    = json::object();
	res["main_info"]                  = content;
	res["displayed_secondary_infos"]  = json::array();
	res["secondary_infos"]            = notes;
	res["entities"]                   = json::object();
	if (params.include_symbols) {
		auto content_symbols = content->get_symbols();
		for (auto& note: notes) {
			auto note_symbols = note.second->get_symbols();
			content_symbols.insert(note_symbols.begin(), note_symbols.end());
			if (note.second->is_displayed()) res["displayed_secondary_infos"].push_back(note.first);
		}
		if (!content_symbols.empty()) {
			json j = content_symbols;
			res["entities"].update(j);
		}
	}
	if (params.include_types) {
		auto content_types = content->get_types();
		for (auto& note: notes) {
			auto note_types = note.second->get_types();
			content_types.insert(note_types.begin(), note_types.end());
		}
		if (!content_types.empty()) {
			json j = content_types;
			res["entities"].update(j);
		}
	}
	return res;
}
