#pragma once

#include "template_file.hpp"

#include <base/box.hpp>
#include <base/visitor.hpp>

namespace dia_app::state {
	/**
	 * @brief Unique identifier of a UI component inside a diagnostic.
	 */
	using ComponentID = u64;

	/**
	 * @brief Identifier of a highlight group assigned to code pieces.
	 */
	using PointerMessageID = u64;

	using EntityID = u64;

	using MessageID = u64;


	class TextComponent;
	class CodeComponent;
	class ConcatComponent;
	class InteractiveComponent;
	class StartLineComponent;
	class CodeBlockComponent;
	class CodeLocationComponent;

	MAKE_VISITOR(Component,
			TextComponent,
			CodeBlockComponent,
			CodeComponent,
			ConcatComponent,
			InteractiveComponent,
			StartLineComponent,
			CodeLocationComponent
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

		virtual ~Component() = default;

		void setParent(CRef<Component> p) { this->parent = p; }

		/**
		 * @brief Apply an interaction to this subtree.
		 */
		virtual void acceptVisitor(ComponentVisitor& visitor) const = 0;

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
	public:
		std::string            content;
		std::vector<MessageID> attached_messages;

		TextComponent(std::string content): Component(), content(std::move(content)) {}

		TextComponent(std::string content, std::vector<MessageID> attached_messages):
			  Component(),
			  content(std::move(content)),
			  attached_messages(std::move(attached_messages)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitTextComponent(*this);
		}

		void reset() final {}
	};

	/**
	 * @brief Leaf component holding a piece of code with highlight pointer_messages.
	 *
	 * Each code piece can participate in multiple highlight pointer_messages that
	 * tie into pointer messages within a code section.
	 */
	class CodeComponent: public Component {
	public:
		std::string                   content;
		std::vector<PointerMessageID> pointer_messages;
		std::vector<MessageID>        attached_messages;

		CodeComponent(
			ComponentID /* id */,
			std::string                   content,
			std::vector<PointerMessageID> pointer_messages,
			std::vector<MessageID>        attached_messages
		):
			  Component(),
			  content(std::move(content)),
			  pointer_messages(std::move(pointer_messages)),
			  attached_messages(std::move(attached_messages)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeComponent(*this);
		}

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

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitConcatComponent(*this);
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
		Box<Component> primary, alternative;

		InteractiveComponent(ComponentID id, Box<Component> primary, Box<Component> alternative):
			  Component(),
			  id(id),
			  primary(std::move(primary)),
			  alternative(std::move(alternative)) {
			this->primary->setParent(this);
			this->alternative->setParent(this);
		}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitInteractiveComponent(*this);
		}

		void reset() final {
			status = Status::Primary;
			this->primary->reset();
			this->alternative->reset();
		}
	};

	/**
	 * @brief Leaf component that inserts a line break and optional line number.
	 *
	 * Line numbers are honored when placed within a code section.
	 */
	class StartLineComponent: public Component {
	public:
		base::Optional<u64> number;

		StartLineComponent(base::Optional<u64> number): Component(), number(number) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitStartLineComponent(*this);
		}

		void reset() final {}
	};

	struct CodeLocation {
		std::string file;
		usize       line;
		usize       column;
	};

	class CodeBlockComponent: public Component {
	private:

	public:
		Box<Component>               content;
		base::Optional<CodeLocation> location;

		CodeBlockComponent(Box<Component> content, base::Optional<CodeLocation> location = {}):
			  Component(),
			  content(std::move(content)),
			  location(std::move(location)) {
			this->content->setParent(this);
		}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeBlockComponent(*this);
		}

		void reset() override { content->reset(); }
	};

	class CodeLocationComponent: public Component {
	public:
		CodeLocation location;

		CodeLocationComponent(CodeLocation location): Component(), location(std::move(location)) {}

		void acceptVisitor(ComponentVisitor& visitor) const final {
			visitor.visitCodeLocationComponent(*this);
		}

		void reset() final {}
	};

	class PointerMessage {
	public:
		std::string type;
		std::string content;
		u64         priority;
	};

	class ExploreEdge {
	public:
		Box<Component> content;
	};

	class Message {
	public:
		template_file::Metadata                         metadata;
		Box<Component>                                  header;
		MBox<Component>                                 description;
		base::HashMap<PointerMessageID, PointerMessage> pointer_messages;
		base::HashMap<std::string, ExploreEdge>         explore_edges;

		Message(
			template_file::Metadata                         metadata,
			Box<Component>                                  header,
			MBox<Component>                                 description,
			base::HashMap<PointerMessageID, PointerMessage> pointer_messages,
			base::HashMap<std::string, ExploreEdge>         explore_edges
		):
			  metadata(std::move(metadata)),
			  header(std::move(header)),
			  description(std::move(description)),
			  pointer_messages(std::move(pointer_messages)),
			  explore_edges(std::move(explore_edges)) {}
	};

	class Diagnostic {
	public:
		std::vector<state::MessageID> displayed_messages;  // The order also matters
		std::vector<Message>          messages;

		Diagnostic(std::vector<state::MessageID> displayed_messages, std::vector<Message> messages):
			  displayed_messages(std::move(displayed_messages)),
			  messages(std::move(messages)) {}
	};
}
