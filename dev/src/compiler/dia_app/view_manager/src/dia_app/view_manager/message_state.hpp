#pragma once

#include "template_file.hpp"

#include <base/box.hpp>
#include <base/visitor.hpp>

namespace dia_app::view_manager {
	/**
	 * @brief Unique identifier of a UI component inside a diagnostic.
	 */
	using ComponentID = u32;

	/**
	 * @brief Identifier of a highlight group assigned to code pieces.
	 */
	using PointerMessageID = u32;

	using EntityID = u32;

	using MessageID = u32;


	struct TextComponent;
	struct CodeComponent;
	struct ConcatComponent;
	struct InteractiveComponent;
	struct StartLineComponent;
	struct CodeBlockComponent;

	MAKE_VISITOR(Component,
			TextComponent,
			CodeBlockComponent,
			CodeComponent,
			ConcatComponent,
			InteractiveComponent,
			StartLineComponent,
		);

	/**
	 * @brief Abstract base of the view-manager component tree.
	 *
	 * Concrete subclasses represent text, code, concatenation, interactive
	 * branch and line breaks. Each component can be serialized either to a
	 * highlighted representation or to a plain-text one, and can react to
	 * interactions.
	 */
	class Component {
	public:
		MCRef<Component> parent;

		Component() = default;

		Component(CRef<Component> parent): parent(parent) {}

		virtual ~Component();

		void setParent(CRef<Component> p) { this->parent = p; }

		/**
		 * @brief Apply an interaction to this subtree.
		 */
		virtual void acceptVisitor(ComponentVisitor& visitor) = 0;

		/**
		 * @brief Reset the internal state of this component.
		 *
		 * Used to revert interactions and return to the initial display.
		 */
		virtual void reset() = 0;
	};

	/**
	 * @brief Leaf component holding simple text.
	 *
	 * Can have associated side entries, but does not support highlighting
	 * (attempting to serialize as highlighted is prohibited).
	 */
	class TextComponent: public Component {
	private:
		ComponentID            id;
		std::string            content;
		std::vector<MessageID> attached_messages;

	public:
		TextComponent(ComponentID id, std::string content, std::vector<MessageID> attached_messages):
			  Component(),
			  id(id),
			  content(std::move(content)),
			  attached_messages(std::move(attached_messages)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitTextComponent(*this); }

		void reset() final {}
	};

	/**
	 * @brief Leaf component holding a piece of code with highlight pointer_messages.
	 *
	 * Each code piece can participate in multiple highlight pointer_messages that
	 * tie into pointer messages within a code section.
	 */
	class CodeComponent: public Component {
	private:
		ComponentID                   id;
		std::string                   content;
		std::vector<PointerMessageID> tags;
		std::vector<MessageID>        attached_messages;

	public:
		CodeComponent(
			ComponentID                   id,
			std::string                   content,
			std::vector<PointerMessageID> tags,
			std::vector<MessageID>        attached_messages
		):
			  Component(),
			  id(id),
			  content(std::move(content)),
			  tags(std::move(tags)),
			  attached_messages(std::move(attached_messages)) {}

		void acceptVisitor(ComponentVisitor& visitor) final { visitor.visitCodeComponent(*this); }

		void reset() final {}
	};

	/**
	 * @brief Node component concatenating multiple child components in order.
	 */
	class ConcatComponent: public Component {
	private:

	public:
		std::vector<Box<Component>> components;

		ConcatComponent(std::vector<Box<Component>> components):
			  Component(),
			  components(std::move(components)) {
			for (auto& comp: this->components) comp->setParent(this);
		}

		void reset() final {
			for (auto& comp: components) comp->reset();
		}
	};

	/**
	 * @brief Node component that toggles between primary and alternative
	 * content upon interaction.
	 */
	class InteractiveComponent: public Component {
	public:
		enum class Status : bool { Primary, Alternative };

	private:
		ComponentID id;
		Status      status = Status::Primary;

	public:
		std::shared_ptr<Component> primary, alternative;
		InteractiveComponent(
			ComponentID                       id,
			const std::shared_ptr<Component>& primary,
			const std::shared_ptr<Component>& alternative
		);

		void acceptVisitor(ComponentVisitor& visitor) final {
			visitor.visitInteractiveComponent(*this);
		}

		void reset() final {
			status = Status::Primary;
			if (this->primary) this->primary->reset();
			if (this->alternative) this->alternative->reset();
		}
	};

	/**
	 * @brief Leaf component that inserts a line break and optional line number.
	 *
	 * Line numbers are honored when placed within a code section.
	 */
	class StartLineComponent: public Component {
	private:
		base::Optional<u32> number;

		friend class GetHlViewVisitor;
		friend class GetNoHlViewVisitor;
		friend class InteractionVisitor;

	public:
		StartLineComponent(base::Optional<u32> number): Component(), number(number) {}

		void acceptVisitor(ComponentVisitor& visitor) final {
			visitor.visitStartLineComponent(*this);
		}

		void reset() final {}
	};

	class CodeBlockComponent: public Component {
	private:
		ComponentID id;

	public:
		Box<Component> content;

		CodeBlockComponent(ComponentID id, Box<Component> content):
			  Component(),
			  id(id),
			  content(std::move(content)) {
			content->setParent(this);
		}

		void acceptVisitor(ComponentVisitor& visitor) final {
			visitor.visitCodeBlockComponent(*this);
		}

		void reset() override { content->reset(); }
	};

	class Message {
	public:
		template_file::Metadata metadata;
		Box<Component> header;
		std::vector<Box<Component>> description;

		Message(
			template_file::Metadata        metadata,
			Box<Component>                 header,
			std::vector<Box<Component>> description
		):
			  metadata(std::move(metadata)),
			  header(std::move(header)),
			  description(std::move(description)) {
		}
	};
}
