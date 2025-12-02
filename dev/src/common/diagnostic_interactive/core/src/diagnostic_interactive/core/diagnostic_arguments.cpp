#include "diagnostic_arguments.hpp"

#include "diagnostic_arguments_forward.hpp"

#include <base/pointers/box.hpp>

DEFAULT_BOX_PTR_DELETER_DEFINITION(dia_int::dia_args::Component);

namespace dia_int::dia_args {
	auto Component::fromJson(const json& elem) -> Box<Component> {
		ASSUME_HAS(elem, "type");
		std::string type = elem["type"];
		if (type == TextComponent::typeName())
			return TextComponent::fromJson(elem);
		else if (type == CodeComponent::typeName())
			return CodeComponent::fromJson(elem);
		else if (type == CodeLocationComponent::typeName())
			return CodeLocationComponent::fromJson(elem);
		else if (type == StartLineComponent::typeName())
			return StartLineComponent::fromJson(elem);
		else if (type == ConcatComponent::typeName())
			return ConcatComponent::fromJson(elem);
		else if (type == PointedComponent::typeName())
			return PointedComponent::fromJson(elem);
		else if (type == VariantComponent::typeName())
			return VariantComponent::fromJson(elem);
		else if (type == LinkComponent::typeName())
			return LinkComponent::fromJson(elem);
		else if (type == EvaluatedTemplateComponent::typeName())
			return EvaluatedTemplateComponent::fromJson(elem);
		else if (type == MessageIDComponent::typeName())
			return MessageIDComponent::fromJson(elem);
		else
			throw ParsingDiagnosticFileError(base::strConcat("Unknown component type '", type, "'.")
			);
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

	json CodeLocationComponent::toJson() const {
		json result;
		result["type"]   = typeName();
		result["file"]   = file;
		result["line"]   = line;
		result["column"] = column;
		return result;
	}

	auto CodeLocationComponent::fromJson(const json& elem) -> Box<CodeLocationComponent> {
		ASSUME_HAS_STR(elem, "file");
		ASSUME_UINT(elem, "line");
		ASSUME_UINT(elem, "column");

		std::string file   = elem["file"];
		u64         line   = elem["line"];
		u64         column = elem["column"];
		return base::makeBox<CodeLocationComponent>(std::move(file), line, column);
	}

	json StartLineComponent::toJson() const {
		json result;
		result["type"] = typeName();
		if (number.has_value()) result["number"] = number.value();
		return result;
	}

	auto StartLineComponent::fromJson(const json& elem) -> Box<StartLineComponent> {
		base::Optional<u64> number;
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
		ASSUME_ARR(elem, "content");
		std::vector<Box<Component>> elements;
		for (auto& el: elem["content"]) elements.push_back(Component::fromJson(el));
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
		for (auto& fpm_json: elem["pointer_messages"])
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

	json LinkComponent::toJson() const {
		json result;
		result["type"]            = typeName();
		result["target_messages"] = target_messages;
		result["content"]         = content->toJson();
		return result;
	}

	auto LinkComponent::fromJson(const json& elem) -> Box<LinkComponent> {
		ASSUME_HAS_STR(elem, "target_messages");
		ASSUME_HAS(elem, "content");
		std::vector<MessageID> target_messages
			= elem["target_messages"].get<std::vector<MessageID>>();
		auto content = Component::fromJson(elem["content"]);
		return base::makeBox<LinkComponent>(std::move(target_messages), std::move(content));
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
		result["template_type"] = template_type;
		result["type"]          = type;
		result["family"]        = family;
		result["name"]          = name;
		return result;
	}

	Metadata Metadata::fromJson(const json& meta_json) {
		ASSUME_HAS_STR(meta_json, "template_type");
		ASSUME_HAS_STR(meta_json, "type");
		ASSUME_HAS_STR(meta_json, "family");
		ASSUME_HAS_STR(meta_json, "name");
		Metadata result;
		result.template_type = meta_json["template_type"];
		result.type          = meta_json["type"];
		result.family        = meta_json["family"];
		result.name          = meta_json["name"];
		return result;
	}

	json ExploreLink::toJson() const {
		json result;
		result["name"]   = name;
		result["params"] = json::object();
		for (const auto& [key, val]: params) result["params"][key] = val->toJson();
		return result;
	}

	ExploreLink ExploreLink::fromJson(const json& edge_json) {
		ASSUME_HAS_STR(edge_json, "name");
		ASSUME_HAS(edge_json, "params");

		ExploreLink result;
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
		for (const auto& [key, val]: arguments) result["params"][key] = val->toJson();

		if (!explore_links.empty()) {
			result["explore_links"] = json::array();
			for (const auto& edge: explore_links) result["explore_links"].push_back(edge.toJson());
		}
		if (!linked_messages.empty()) {
			result["attached_messages"] = json::array();
			for (const auto& info_id: linked_messages)
				result["attached_messages"].push_back(info_id);
		}
		return result;
	}

	Message Message::fromJson(const json& msg_json) {
		ASSUME_HAS(msg_json, "metadata");
		ASSUME_HAS(msg_json, "params");

		Message result;
		result.metadata  = Metadata::fromJson(msg_json["metadata"]);
		result.arguments = jsonToMap<Box<Component>>(msg_json["params"], [](const json& el) {
			return Component::fromJson(el);
		});

		if (msg_json.contains("explore_links")) {
			ASSUME_ARR(msg_json, "explore_links");
			for (const auto& edge_json: msg_json["explore_links"])
				result.explore_links.push_back(ExploreLink::fromJson(edge_json));
		}

		if (msg_json.contains("attached_messages")) {
			ASSUME_ARR(msg_json, "attached_messages");
			result.linked_messages = msg_json["attached_messages"].get<std::vector<MessageID>>();
		}

		return result;
	}

	json Diagnostic::toJson() const {
		json result;
		result["main_message"]    = main_message.toJson();
		result["linked_messages"] = json::object();

		for (const auto& [info_id, msg]: linked_messages)
			result["linked_messages"][info_id] = msg.toJson();

		return result;
	}

	Diagnostic Diagnostic::fromJson(const json& diag_json) {
		ASSUME_HAS(diag_json, "main_message");
		ASSUME_HAS(diag_json, "linked_messages");

		Diagnostic result;
		result.main_message = Message::fromJson(diag_json["main_message"]);

		ASSUME_HAS(diag_json, "linked_messages");
		for (const auto& [info_id, msg_json]: diag_json["linked_messages"].items())
			result.linked_messages.put(info_id, Message::fromJson(msg_json));

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
		if (message_id.has_value()) result["message_id"] = message_id.value();
		result["pointer_message_id"] = pointer_message_id;
		return result;
	}
}
