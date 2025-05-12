#include "interactive_message.hpp"

#include "interactive_logger.hpp"

using nlohmann::json;

json dia::InteractiveMessage::tojson() const {
	const SerializationParams& params = InteractiveLogger::params();
	json                       res{};
	if (params.include_symbols) {
		auto content_symbols = content->get_symbols();
		for (auto& note: notes) {
			auto note_symbols = note->get_symbols();
			content_symbols.insert(note_symbols.begin(), note_symbols.end());
		}
		res["symbols"] = content_symbols;
	}
	if (params.include_types) {
		auto content_types = content->get_types();
		for (auto& note: notes) {
			auto note_types = note->get_types();
			content_types.insert(note_types.begin(), note_types.end());
		}
		res["types"] = content_types;
	}
	res["content"] = content;
	res["notes"]   = notes;
	return res;
}

json dia::OperatorNotFound::Params::tojson() {
	json lhs_type, rhs_type;
	lhs_type["type"] = std::to_string(lhs->expression_type.getType().customPerfectHash());
	rhs_type["type"] = std::to_string(rhs->expression_type.getType().customPerfectHash());
	return json{ { "operator", op.strView() },
		         { "left_type", lhs_type },
		         { "right_type", rhs_type } };
}
