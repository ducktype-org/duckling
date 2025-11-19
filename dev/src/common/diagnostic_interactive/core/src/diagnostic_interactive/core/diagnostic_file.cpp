#include "diagnostic_file.hpp"

namespace dia_app::dia_file {
	auto Component::fromJson(const json& elem) -> Box<Component> {
		ASSUME_HAS(elem, "type");
		std::string type = elem["type"];
		if (type == TextComponent::typeName())
			return TextComponent::fromJson(elem);
		else if (type == CodeComponent::typeName())
			return CodeComponent::fromJson(elem);
		else if (type == CodeWithLocationComponent::typeName())
			return CodeWithLocationComponent::fromJson(elem);
		else if (type == StartLineComponent::typeName())
			return StartLineComponent::fromJson(elem);
		else if (type == ConcatComponent::typeName())
			return ConcatComponent::fromJson(elem);
		else if (type == PointedComponent::typeName())
			return PointedComponent::fromJson(elem);
		else if (type == VariantComponent::typeName())
			return VariantComponent::fromJson(elem);
		else if (type == EntityComponent::typeName())
			return EntityComponent::fromJson(elem);
		else if (type == EvaluatedTemplateComponent::typeName())
			return EvaluatedTemplateComponent::fromJson(elem);
		else if (type == MessageIDComponent::typeName())
			return MessageIDComponent::fromJson(elem);
		else
			CORE_PANIC("Unknown component type '{}'.", type);
	}

	json TextComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["content"] = content;
		return result;
	}

	auto TextComponent::fromJson(const json& elem) -> Box<TextComponent> {
		ASSUME_HAS_STR(elem, "content");
		std::string content = elem["content"];
		return base::makeBox<TextComponent>(std::move(content));
	}

	json CodeComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["content"] = content;
		return result;
	}

	auto CodeComponent::fromJson(const json& elem) -> Box<CodeComponent> {
		ASSUME_HAS_STR(elem, "content");
		std::string content = elem["content"];
		return base::makeBox<CodeComponent>(std::move(content));
	}

	json CodeWithLocationComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["file"]    = file;
		result["line"]    = line;
		result["column"]  = column;
		result["content"] = content->toJson();
		return result;
	}

	auto CodeWithLocationComponent::fromJson(const json& elem) -> Box<CodeWithLocationComponent> {
		ASSUME_HAS_STR(elem, "file");
		ASSUME_UINT(elem, "line");
		ASSUME_UINT(elem, "column");
		ASSUME_HAS(elem, "content");

		std::string file    = elem["file"];
		u64         line    = elem["line"];
		u64         column  = elem["column"];
		auto        content = Component::fromJson(elem["content"]);
		return base::makeBox<CodeWithLocationComponent>(
			std::move(file), line, column, std::move(content)
		);
	}

	json StartLineComponent::toJson() const {
		json result;
		result["type"] = typeName();
		if (number.has_value()) result["number"] = number.value();
		return result;
	}

	auto StartLineComponent::fromJson(const json& elem) -> Box<StartLineComponent> {
		base::Optional<u32> number;
		if (elem.contains("number")) {
			ASSUME_UINT(elem, "number");
			number = elem["number"];
		}
		return base::makeBox<StartLineComponent>(number);
	}

	json ConcatComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["content"] = json::array();
		for (const auto& elem: elements) result["content"].push_back(elem->toJson());
		return result;
	}

	auto ConcatComponent::fromJson(const json& elem) -> Box<ConcatComponent> {
		CORE_ASSERT(elem.is_array(), "Concat component must be an array.");
		std::vector<Box<Component>> elements;
		for (auto& el: elem) elements.push_back(Component::fromJson(el));
		return base::makeBox<ConcatComponent>(std::move(elements));
	}

	json PointedComponent::toJson() const {
		json result;
		result["type"]             = typeName();
		result["content"]          = content->toJson();
		result["pointer_messages"] = json::array();
		for (const auto& pm: pointer_messages) result["pointer_messages"].push_back(pm.toJson());
		return result;
	}

	auto PointedComponent::fromJson(const json& elem) -> Box<PointedComponent> {
		ASSUME_HAS(elem, "content");
		ASSUME_ARR(elem, "pointer_messages");
		auto content = Component::fromJson(elem["content"]);

		std::vector<PointerMessage> pointer_messages;
		for (auto& fpm_json: elem["foreign_pointer_messages"])
			pointer_messages.push_back(PointerMessage::fromJson(fpm_json));

		return base::makeBox<PointedComponent>(std::move(content), std::move(pointer_messages));
	}

	json VariantComponent::toJson() const {
		json result;
		result["type"]        = typeName();
		result["content"]     = content->toJson();
		result["alt_content"] = alt_content->toJson();
		return result;
	}

	auto VariantComponent::fromJson(const json& elem) -> Box<VariantComponent> {
		ASSUME_HAS(elem, "content");
		ASSUME_HAS(elem, "alt_content");
		auto content     = Component::fromJson(elem["content"]);
		auto alt_content = Component::fromJson(elem["alt_content"]);
		return base::makeBox<VariantComponent>(std::move(content), std::move(alt_content));
	}

	json EntityComponent::toJson() const {
		json result;
		result["type"]      = typeName();
		result["refers_to"] = entity_id;
		result["content"]   = content->toJson();
		return result;
	}

	auto EntityComponent::fromJson(const json& elem) -> Box<EntityComponent> {
		ASSUME_HAS_STR(elem, "refers_to");
		ASSUME_HAS(elem, "content");
		MessageID entity_id = elem["refers_to"];
		auto      content   = Component::fromJson(elem["content"]);
		return base::makeBox<EntityComponent>(std::move(content), std::move(entity_id));
	}

	json EvaluatedTemplateComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["info_id"] = message_id;
		return result;
	}

	auto EvaluatedTemplateComponent::fromJson(const json& elem) -> Box<EvaluatedTemplateComponent> {
		ASSUME_HAS_STR(elem, "info_id");
		std::string message_id = elem["info_id"];
		return base::makeBox<EvaluatedTemplateComponent>(std::move(message_id));
	}

	json MessageIDComponent::toJson() const {
		json result;
		result["type"]    = typeName();
		result["info_id"] = message_id;
		return result;
	}

	auto MessageIDComponent::fromJson(const json& elem) -> Box<MessageIDComponent> {
		ASSUME_HAS_STR(elem, "info_id");
		std::string message_id = elem["info_id"];
		return base::makeBox<MessageIDComponent>(std::move(message_id));
	}

	json Metadata::toJson() const {
		json result;
		result["type"]   = type;
		result["family"] = family;
		result["name"]   = name;
		return result;
	}

	Metadata Metadata::fromJson(const json& meta_json) {
		ASSUME_HAS_STR(meta_json, "type");
		ASSUME_HAS_STR(meta_json, "family");
		ASSUME_HAS_STR(meta_json, "name");
		Metadata result;
		result.type   = meta_json["type"];
		result.family = meta_json["family"];
		result.name   = meta_json["name"];
		return result;
	}

	json ExploreEdge::toJson() const {
		json result;
		result["name"]   = name;
		result["params"] = json::object();
		for (const auto& [key, val]: params) result["params"][key] = val->toJson();
		return result;
	}

	ExploreEdge ExploreEdge::fromJson(const json& edge_json) {
		ASSUME_HAS_STR(edge_json, "name");
		ASSUME_HAS_STR(edge_json, "info_id");
		ASSUME_HAS(edge_json, "params");

		ExploreEdge result;
		result.name   = edge_json["name"];
		result.params = jsonToMap<Box<Component>>(edge_json["params"], [](const json& el) {
			return Component::fromJson(el);
		});
		return result;
	}

	json Message::toJson() const {
		json result;
		result["metadata"] = metadata.toJson();
		result["params"]   = json::object();
		for (const auto& [key, val]: params) result["params"][key] = val->toJson();

		if (!explore_edges.empty()) {
			result["explore_edges"] = json::array();
			for (const auto& edge: explore_edges) result["explore_edges"].push_back(edge.toJson());
		}
		return result;
	}

	Message Message::fromJson(const json& msg_json) {
		ASSUME_HAS(msg_json, "metadata");
		ASSUME_HAS(msg_json, "params");

		Message result;
		result.metadata = Metadata::fromJson(msg_json["metadata"]);
		result.params   = jsonToMap<Box<Component>>(msg_json["params"], [](const json& el) {
            return Component::fromJson(el);
        });

		if (msg_json.contains("explore_edges")) {
			ASSUME_ARR(msg_json, "explore_edges");
			for (const auto& edge_json: msg_json["explore_edges"])
				result.explore_edges.push_back(ExploreEdge::fromJson(edge_json));
		}

		return result;
	}

	json Entity::toJson() const {
		json result;
		result["assoc_infos"] = json::array();
		for (const auto& info_id: assoc_infos) result["assoc_infos"].push_back(info_id);
		return result;
	}

	Entity Entity::fromJson(const json& entity_json) {
		ASSUME_HAS(entity_json, "assoc_infos");
		ASSUME_ARR(entity_json, "assoc_infos");

		Entity result;
		for (const auto& info_id_json: entity_json["assoc_infos"]) {
			CORE_ASSERT(info_id_json.is_string(), "Info ID in assoc_infos must be a string.");
			result.assoc_infos.push_back(info_id_json);
		}
		return result;
	}

	json Thread::toJson() const {
		json result;
		result["main_message"]                = main_message.toJson();
		result["displayed_attached_messages"] = displayed_attached_messages;
		result["attached_messages"]           = json::object();

		for (const auto& [info_id, msg]: attached_messages)
			result["attached_messages"][info_id] = msg.toJson();

		result["entities"] = json::object();

		for (const auto& [entity_id, entity]: entities)
			result["entities"][entity_id] = entity.toJson();

		return result;
	}

	Thread Thread::fromJson(const json& thread_json) {
		ASSUME_HAS(thread_json, "main_message");
		ASSUME_ARR(thread_json, "displayed_attached_messages");
		ASSUME_HAS(thread_json, "attached_messages");
		ASSUME_HAS(thread_json, "entities");

		Thread result;
		result.main_message                = Message::fromJson(thread_json["main_message"]);
		result.displayed_attached_messages = thread_json["displayed_attached_messages"];

		ASSUME_HAS(thread_json, "attached_messages");
		for (const auto& [info_id, msg_json]: thread_json["attached_messages"].items())
			result.attached_messages.put(info_id, Message::fromJson(msg_json));

		ASSUME_HAS(thread_json, "entities");
		for (const auto& [entity_id, entity_json]: thread_json["entities"].items())
			result.entities.put(entity_id, Entity::fromJson(entity_json));

		return result;
	}

	PointerMessage PointerMessage::fromJson(const json& elem) {
		ASSUME_HAS_STR(elem, "pointer_message_id");

		PointerMessageID          pointer_message_id = elem["pointer_message_id"];
		base::Optional<MessageID> message_id_opt;

		if (elem.contains("message_id")) {
			ASSUME_HAS_STR(elem, "message_id");
			message_id_opt = elem["message_id"];
		}

		return { std::move(pointer_message_id), std::move(message_id_opt) };
	}

	json PointerMessage::toJson() const {
		json result;
		result["message_id"]         = message_id;
		result["pointer_message_id"] = pointer_message_id;
		return result;
	}
}
