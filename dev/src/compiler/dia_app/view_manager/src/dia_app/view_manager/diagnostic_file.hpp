#pragma once
#include "utils.hpp"

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/visitor.hpp>

#include <json/json.hpp>


namespace dia_app::dia_file {

	struct Component;

	struct TextComponent;
	struct CodeComponent;
	struct CodeWithLocationComponent;
	struct StartLineComponent;
	struct ConcatComponent;
	struct PointedComponent;
	struct VariantComponent;
	struct EntityComponent;
	struct TemplateComponent;
	struct LazyComponent;


	MAKE_VISITOR(Component,
			TextComponent,
			CodeComponent,
            CodeWithLocationComponent,
            StartLineComponent,
            ConcatComponent,
            PointedComponent,
            VariantComponent,
            EntityComponent,
            TemplateComponent,
            LazyComponent
		);

	class ToComponentVisitor;

	using MessageID        = std::string;
	using PointerMessageID = std::string;
	using EntityID         = std::string;

	/**
	 * @brief A base class display element.
	 *
	 * A display element represents a message fragment inside a diagnostic
	 * file or inside the view constructor's internal representation.
	 * It can define the text or code to be displayed, additional metadata,
	 * interactive content, or represent a lazily fetched subtree of display
	 * elements.
	 *
	 */
	struct Component {
		virtual ~Component() = default;

		virtual void acceptVisitor(ComponentVisitor& visitor) const = 0;

		[[nodiscard]] virtual json toJson() const = 0;

		static auto fromJson(const json& elem) -> Box<Component>;
	};

	/**
	 * @brief A display element representing simple text.
	 */
	struct TextComponent: public Component {
		std::string content;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitTextComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]    = "text";
			result["content"] = content;
			return result;
		}

		static auto fromJson(const json& elem) -> Box<TextComponent> {
			ASSUME_HAS_STR(elem, "content");

			auto result     = base::makeBox<TextComponent>();
			result->content = elem["content"];
			return result;
		}
	};

	struct CodeComponent: public Component {
		std::string content;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]    = "code";
			result["content"] = content;
			return result;
		}

		static auto fromJson(const json& elem) -> Box<CodeComponent> {
			ASSUME_HAS_STR(elem, "content");

			auto result     = base::makeBox<CodeComponent>();
			result->content = elem["content"];
			return result;
		}
	};

	struct CodeWithLocationComponent: public Component {
		std::string    file;
		u64            line;
		u64            column;
		Box<Component> content;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeWithLocationComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]    = "code_with_location";
			result["file"]    = file;
			result["line"]    = line;
			result["column"]  = column;
			result["content"] = content->toJson();
			return result;
		}

		static auto fromJson(const json& elem) -> Box<CodeWithLocationComponent> {
			ASSUME_HAS_STR(elem, "file");
			ASSUME_UINT(elem, "line");
			ASSUME_UINT(elem, "column");
			ASSUME_HAS(elem, "content");

			// @TODO check if the default constructor will work here
			auto result     = base::makeBox<CodeWithLocationComponent>();
			result->file    = elem["file"];
			result->line    = elem["line"];
			result->column  = elem["column"];
			result->content = Component::fromJson(elem["content"]);
			return result;
		}
	};

	struct StartLineComponent: public Component {
		// Line number.
		base::Optional<u32> number;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitStartLineComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"] = "start_line";
			if (number.has_value()) result["number"] = number.value();
			return result;
		}

		static auto fromJson(const json& elem) -> Box<StartLineComponent> {
			auto result = base::makeBox<StartLineComponent>();
			if (elem.contains("number")) {
				ASSUME_UINT(elem, "number");
				result->number = elem["number"];
			}
			return result;
		}
	};

	struct ConcatComponent: public Component {
		std::vector<Box<Component>> elements;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitConcatComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]    = "concat";
			result["content"] = json::array();
			for (const auto& elem: elements) result["content"].push_back(elem->toJson());
			return result;
		}

		static auto fromJson(const json& elem) -> Box<ConcatComponent> {
			auto result = base::makeBox<ConcatComponent>();

			CORE_ASSERT(elem.is_array(), "Concat component must be an array.");
			for (auto& el: elem) result->elements.push_back(Component::fromJson(el));
		}
	};

	struct PointedComponent: public Component {
		Box<Component>                content;
		std::vector<PointerMessageID> pointer_messages;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitPointedComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]             = "pointed";
			result["content"]          = content->toJson();
			result["pointer_messages"] = json::array();
			for (const auto& pm: pointer_messages) result["pointer_messages"].push_back(pm);
			return result;
		}

		static auto fromJson(const json& elem) -> Box<PointedComponent> {
			ASSUME_HAS(elem, "content");
			auto result     = base::makeBox<PointedComponent>();
			result->content = Component::fromJson(elem["content"]);
			ASSUME_ARR(elem, "pointer_messages");
			for (auto& pm: elem["pointer_messages"]) {
				CORE_ASSERT(pm.is_string(), "Pointer message ID must be a string.");
				result->pointer_messages.push_back(pm);
			}
			return result;
		}
	};

	struct VariantComponent: public Component {
		Box<Component> content;
		Box<Component> alt_content;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitVariantComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]        = "variant";
			result["content"]     = content->toJson();
			result["alt_content"] = alt_content->toJson();
			return result;
		}

		static auto fromJson(const json& elem) -> Box<VariantComponent> {
			ASSUME_HAS(elem, "content");
			ASSUME_HAS(elem, "alt_content");

			auto result         = base::makeBox<VariantComponent>();
			result->content     = Component::fromJson(elem["content"]);
			result->alt_content = Component::fromJson(elem["alt_content"]);
			return result;
		}
	};

	struct EntityComponent: public Component {
		Box<Component> content;
		MessageID      entity_id;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitEntityComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]      = "entity";
			result["refers_to"] = entity_id;
			result["content"]   = content->toJson();
			return result;
		}

		static auto fromJson(const json& elem) -> Box<EntityComponent> {
			ASSUME_HAS_STR(elem, "refers_to");
			ASSUME_HAS(elem, "content");

			auto result       = base::makeBox<EntityComponent>();
			result->entity_id = elem["refers_to"];
			result->content   = Component::fromJson(elem["content"]);
			return result;
		}
	};

	struct TemplateComponent: public Component {
		std::string info_id;

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitTemplateComponent(*this);
		}

		[[nodiscard]] json toJson() const override {
			json result;
			result["type"]    = "template";
			result["info_id"] = info_id;
			return result;
		}

		static auto fromJson(const json& elem) -> Box<TemplateComponent> {
			ASSUME_HAS_STR(elem, "info_id");

			auto result     = base::makeBox<TemplateComponent>();
			result->info_id = elem["info_id"];
			return result;
		}
	};

	// @TODO
	struct ToTextVisitor: public ComponentVisitor {};

	struct Metadata {
		std::string type;
		std::string family;
		std::string name;

		[[nodiscard]] json toJson() const {
			json result;
			result["type"]   = type;
			result["family"] = family;
			result["name"]   = name;
			return result;
		}

		static Metadata fromJson(const json& meta_json) {
			ASSUME_HAS_STR(meta_json, "type");
			ASSUME_HAS_STR(meta_json, "family");
			ASSUME_HAS_STR(meta_json, "name");
			Metadata result;
			result.type   = meta_json["type"];
			result.family = meta_json["family"];
			result.name   = meta_json["name"];
			return result;
		}
	};

	struct ExploreEdge {
		MessageID                                  info_id;
		std::string                                name;
		base::HashMap<std::string, Box<Component>> params;

		[[nodiscard]] json toJson() const {
			json result;
			result["name"]    = name;
			result["info_id"] = info_id;
			result["params"]  = json::object();
			for (const auto& [key, val]: params) result["params"][key] = val->toJson();
			return result;
		}

		static ExploreEdge fromJson(const json& edge_json) {
			ASSUME_HAS_STR(edge_json, "name");
			ASSUME_HAS_STR(edge_json, "info_id");
			ASSUME_HAS(edge_json, "params");

			ExploreEdge result;
			result.name    = edge_json["name"];
			result.info_id = edge_json["info_id"];
			result.params  = jsonToMap<Box<Component>>(edge_json["params"], [](const json& el) {
                return Component::fromJson(el);
            });
			return result;
		}
	};

	struct Message {
		Metadata                                   metadata;
		base::HashMap<std::string, Box<Component>> params;
		std::vector<ExploreEdge>                   explore_edges;

		[[nodiscard]] json toJson() const {
			json result;
			result["metadata"] = metadata.toJson();
			result["params"]   = json::object();
			for (const auto& [key, val]: params) result["params"][key] = val->toJson();

			if (!explore_edges.empty()) {
				result["explore_edges"] = json::array();
				for (const auto& edge: explore_edges)
					result["explore_edges"].push_back(edge.toJson());
			}
			return result;
		}

		static Message fromJson(const json& msg_json) {
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
	};

	struct Entity {
		std::vector<MessageID> assoc_infos;

		[[nodiscard]] json toJson() const {
			json result;
			result["assoc_infos"] = json::array();
			for (const auto& info_id: assoc_infos) result["assoc_infos"].push_back(info_id);
			return result;
		}

		static Entity fromJson(const json& entity_json) {
			ASSUME_HAS(entity_json, "assoc_infos");
			ASSUME_ARR(entity_json, "assoc_infos");

			Entity result;
			for (const auto& info_id_json: entity_json["assoc_infos"]) {
				CORE_ASSERT(info_id_json.is_string(), "Info ID in assoc_infos must be a string.");
				result.assoc_infos.push_back(info_id_json);
			}
			return result;
		}
	};

	struct Thread {
		Message                           main_message;
		std::vector<MessageID>            displayed_attached_messages;
		base::HashMap<MessageID, Message> attached_messages;
		base::HashMap<EntityID, Entity>   entities;

		[[nodiscard]] json toJson() const {
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

		static Thread fromJson(const json& thread_json) {
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
	};
}  // namespace dia_app::dia_file
