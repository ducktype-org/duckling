#pragma once

#include "template_file.hpp"

#include <diagnostic_interactive/core/common_classes.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/visitor.hpp>
#include <base/pointers/box.hpp>

#include <iostream>

namespace dia_int::state {
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

		Component(CRef<Component> parent);

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

		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;
	};

	/**
	 * @brief Leaf component holding simple text.
	 *
	 * Can have associated side entries, but does not support highlighting
	 * (attempting to serialize as highlighted is prohibited).
	 */
	class TextComponent final: public Component {
	public:
		std::string            content;
		std::vector<MessageID> linked_messages;

		TextComponent(std::string content);

		TextComponent(std::string content, std::vector<MessageID> attached_messages);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final {}

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	/**
	 * @brief Leaf component holding a piece of code with highlight pointer_messages.
	 *
	 * Each code piece can participate in multiple highlight pointer_messages that
	 * tie into pointer messages within a code section.
	 */
	class CodeComponent final: public Component {
	public:
		std::string                   content;
		std::vector<PointerMessageID> pointer_messages;
		std::vector<MessageID>        attached_messages;

		CodeComponent(
			ComponentID /* id */,
			const std::string&            content,
			std::vector<PointerMessageID> pointer_messages,
			std::vector<MessageID>        attached_messages
		);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final {}

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	/**
	 * @brief Node component concatenating multiple child components in order.
	 */
	class ConcatComponent final: public Component {
	private:

	public:
		std::vector<Box<Component>> components;

		ConcatComponent(std::vector<Box<Component>> components);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final;

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	/**
	 * @brief Node component that toggles between primary and alternative
	 * content upon interaction.
	 */
	class InteractiveComponent final: public Component {
	public:
		enum class Status : bool { Primary, Alternative };

	private:
		ComponentID id;
		Status      status = Status::Primary;

	public:
		Box<Component> primary, alternative;

		InteractiveComponent(ComponentID id, Box<Component> primary, Box<Component> alternative);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final;

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	/**
	 * @brief Leaf component that inserts a line break and optional line number.
	 *
	 * Line numbers are honored when placed within a code section.
	 */
	class StartLineComponent final: public Component {
	public:
		base::Optional<u64> number;

		StartLineComponent(base::Optional<u64> number);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final {}

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	class CodeBlockComponent final: public Component {
	private:

	public:
		Box<Component>               content;
		base::Optional<CodeLocation> location;

		CodeBlockComponent(Box<Component> content, base::Optional<CodeLocation> location = {});

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() override { content->reset(); }

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	class CodeLocationComponent final: public Component {
	public:
		CodeLocation location;

		CodeLocationComponent(CodeLocation location);

		void acceptVisitor(ComponentVisitor& visitor) const final;

		void reset() final {}

		void debugPrint(std::ostream& out, usize indent = 0) const override;
	};

	class PointerMessage final {
	public:
		std::string type;
		std::string content;
		u64         priority;
	};

	class ExploreEdge final {
	public:
		Box<Component> content;
	};

	class Message final {
	public:
		template_file::Metadata                         metadata;
		Box<Component>                                  header;
		MBox<Component>                                 description;
		base::HashMap<PointerMessageID, PointerMessage> pointer_messages;
		base::HashMap<std::string, ExploreEdge>         explore_links;

		Message(
			template_file::Metadata                         metadata,
			Box<Component>                                  header,
			MBox<Component>                                 description,
			base::HashMap<PointerMessageID, PointerMessage> pointer_messages,
			base::HashMap<std::string, ExploreEdge>         explore_links
		);

		/**
		 * @brief Returns message IDs in the order they appear in the message links.
		 * Useful utlity.
		 *
		 * Traverses header first, then description, then explore_links.
		 * Duplicate IDs are included only once (first occurrence).
		 */
		std::vector<MessageID> getOrderedMessageLinks() const;

		void debugPrint(std::ostream& out) const;
	};

	class Diagnostic final {
	public:
		std::vector<state::MessageID> displayed_messages;  // The order also matters
		std::vector<Message>          messages;

		Diagnostic(std::vector<state::MessageID> displayed_messages, std::vector<Message> messages):
			  displayed_messages(std::move(displayed_messages)),
			  messages(std::move(messages)) {}

		void debugPrint(std::ostream& out) const;
	};
}
