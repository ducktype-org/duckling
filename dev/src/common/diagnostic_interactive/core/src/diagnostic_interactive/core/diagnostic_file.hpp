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
	struct CodeLocationComponent;
	struct StartLineComponent;
	struct ConcatComponent;
	struct PointedComponent;
	struct VariantComponent;
	struct EntityComponent;
	struct EvaluatedTemplateComponent;
	struct MessageIDComponent;


	MAKE_VISITOR(Component,
			TextComponent,
			CodeComponent,
            CodeLocationComponent,
            StartLineComponent,
            ConcatComponent,
            PointedComponent,
            VariantComponent,
            EntityComponent,
            EvaluatedTemplateComponent,
			MessageIDComponent
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

		static Box<Component> fromJson(const json& elem);

		[[nodiscard]] virtual std::string_view getTypeName() const = 0;
	};

	/**
	 * @brief A display element representing simple text.
	 */
	struct TextComponent: public Component {
		std::string content;

		TextComponent() = default;

		TextComponent(std::string content): content(std::move(content)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitTextComponent(*this);
		}

		[[nodiscard]] json toJson() const final;

		[[nodiscard]] static std::string_view typeName() { return "text"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<TextComponent> fromJson(const json& elem);
	};

	struct CodeComponent: public Component {
		std::string content;

		CodeComponent() = default;

		CodeComponent(std::string content): content(std::move(content)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "code"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<CodeComponent> fromJson(const json& elem);
	};

	struct CodeLocationComponent: public Component {
		std::string file;
		u64         line;
		u64         column;

		CodeLocationComponent(std::string file, u64 line, u64 column):
			  file(std::move(file)),
			  line(line),
			  column(column) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeLocationComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "code_location"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<CodeLocationComponent> fromJson(const json& elem);
	};

	struct StartLineComponent: public Component {
		// Line number.
		base::Optional<u64> number;

		StartLineComponent() = default;

		StartLineComponent(base::Optional<u64> number): number(number) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitStartLineComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "start_line"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<StartLineComponent> fromJson(const json& elem);
	};

	struct ConcatComponent: public Component {
		std::vector<Box<Component>> elements;

		ConcatComponent() = default;

		ConcatComponent(std::vector<Box<Component>> elements): elements(std::move(elements)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitConcatComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "concat"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<ConcatComponent> fromJson(const json& elem);
	};

	struct PointerMessage {
		base::Optional<MessageID> message_id;
		PointerMessageID          pointer_message_id;

		PointerMessage(PointerMessageID pointer_message_id, base::Optional<MessageID> message_id):
			  message_id(std::move(message_id)),
			  pointer_message_id(std::move(pointer_message_id)) {}

		bool operator==(const PointerMessage& other) const {
			return message_id == other.message_id && pointer_message_id == other.pointer_message_id;
		}

		[[nodiscard]] json toJson() const;

		static PointerMessage fromJson(const json& elem);
	};

	struct PointedComponent: public Component {
		Box<Component>              content;
		std::vector<PointerMessage> pointer_messages;

		PointedComponent(Box<Component> content, std::vector<PointerMessage> pointer_messages):
			  content(std::move(content)),
			  pointer_messages(std::move(pointer_messages)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitPointedComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "pointed"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<PointedComponent> fromJson(const json& elem);
	};

	struct VariantComponent: public Component {
		Box<Component> content;
		Box<Component> alt_content;

		VariantComponent(Box<Component> content, Box<Component> alt_content):
			  content(std::move(content)),
			  alt_content(std::move(alt_content)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitVariantComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "variant"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<VariantComponent> fromJson(const json& elem);
	};

	struct EntityComponent: public Component {
		Box<Component> content;
		EntityID       entity_id;

		EntityComponent(Box<Component> content, EntityID entity_id):
			  content(std::move(content)),
			  entity_id(std::move(entity_id)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitEntityComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "entity"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<EntityComponent> fromJson(const json& elem);
	};

	/**
	 * @brief
	 *
	 */
	struct EvaluatedTemplateComponent: public Component {
		std::string message_id;

		EvaluatedTemplateComponent(std::string message_id): message_id(std::move(message_id)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitEvaluatedTemplateComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "evaluated_template"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<EvaluatedTemplateComponent> fromJson(const json& elem);
	};

	struct MessageIDComponent: public Component {
		MessageID message_id;

		MessageIDComponent(MessageID message_id): message_id(std::move(message_id)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitMessageIDComponent(*this);
		}

		[[nodiscard]] json toJson() const override;

		[[nodiscard]] static std::string_view typeName() { return "message_id"; }
		[[nodiscard]] std::string_view getTypeName() const override { return typeName(); }

		static Box<MessageIDComponent> fromJson(const json& elem);
	};

	struct Metadata {
		std::string template_type;
		std::string type;
		std::string family;
		std::string name;

		[[nodiscard]] json toJson() const;

		static Metadata fromJson(const json& meta_json);
	};

	struct ExploreEdge {
		std::string                                name;
		base::HashMap<std::string, Box<Component>> params;

		[[nodiscard]] json toJson() const;

		static ExploreEdge fromJson(const json& edge_json);
	};

	struct Message {
		Metadata                                   metadata;
		base::HashMap<std::string, Box<Component>> arguments;
		std::vector<ExploreEdge>                   explore_edges;
		std::vector<MessageID>                     attached_messages;

		[[nodiscard]] json toJson() const;

		static Message fromJson(const json& msg_json);
	};

	struct Entity {
		std::vector<MessageID> attached_messages;

		[[nodiscard]] json toJson() const;

		static Entity fromJson(const json& entity_json);
	};

	struct Thread {
		Message                           main_message;
		base::HashMap<MessageID, Message> additional_messages;
		base::HashMap<EntityID, Entity>   entities;

		[[nodiscard]] json toJson() const;

		static Thread fromJson(const json& thread_json);
	};
}  // namespace dia_app::dia_file

namespace std {
	template<>
	struct hash<dia_app::dia_file::PointerMessage> {
		std::size_t operator()(const dia_app::dia_file::PointerMessage& k) const {
			std::size_t h1 = std::hash<std::string>{}(k.pointer_message_id);
			std::size_t h2 = 0;
			if (k.message_id) h2 = std::hash<std::string>{}(*k.message_id);
			return h1 ^ (h2 << 1);
		}
	};
}
