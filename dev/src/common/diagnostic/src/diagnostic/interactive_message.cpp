#include "interactive_message.hpp"

#include "interactive_logger.hpp"


using nlohmann::json;

json dia::InteractiveMessage::tojson() const {
	const SerializationParams& params = InteractiveLogger::params();
	json                       res    = json::object();
	res["main_info"]                  = content;
	res["displayed_secondary_infos"]  = notes;
	res["secondary_infos"]            = json::array();
	res["entities"]                   = json::object();
	if (params.include_symbols) {
		auto content_symbols = content->get_symbols();
		for (auto& note: notes) {
			auto note_symbols = note->get_symbols();
			content_symbols.insert(note_symbols.begin(), note_symbols.end());
		}
		if (!content_symbols.empty()) {
			json j = content_symbols;
			res["entities"].update(j);
		}
	}
	if (params.include_types) {
		auto content_types = content->get_types();
		for (auto& note: notes) {
			auto note_types = note->get_types();
			content_types.insert(note_types.begin(), note_types.end());
		}
		if (!content_types.empty()) {
			json j = content_types;
			res["entities"].update(j);
		}
	}
	return res;
}

json dia::OperatorNotFound::Params::tojson() {
	json lhs_type, rhs_type;
	// TODO: make this better, eg. creating class for entities
	json code       = json::object();
	code["type"]    = "grouping";
	code["content"] = lhs->expression_type.getType().toString();
	lhs_type["refers_to"]
		= std::to_string(lhs->expression_type.getType().queryUnstablePerfectHash());
	lhs_type["content"] = code;
	lhs_type["type"]    = "entity";
	code["content"]     = rhs->expression_type.getType().toString();
	rhs_type["refers_to"]
		= std::to_string(rhs->expression_type.getType().queryUnstablePerfectHash());
	rhs_type["content"] = code;
	rhs_type["type"]    = "entity";
	// TODO: add types to types.
	return json{ { "operator", op.strView() },
		         { "left_type", lhs_type },
		         { "right_type", rhs_type },
		         { "is_static", "false" },
		         { "has_expanded_left_type", "false" },
		         { "has_expanded_right_type", "false" } };
}
