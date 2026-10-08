// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "diagnostic_state.hpp"

#include <unordered_set>

namespace dia::state {
	static std::string expandTabs(const std::string& s, usize tab_width = 4) {
		std::string out;
		out.reserve(s.size());  // small optimization

		for (char c: s)
			if (c == '\t')
				out.append(tab_width, ' ');  // append N spaces
			else
				out.push_back(c);

		return out;
	}

	void TextComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "TextComponent(" << content << ")";
		if (!linked_messages.empty()) {
			out << " [attached: ";
			for (size_t i = 0; i < linked_messages.size(); ++i)
				out << linked_messages[i] << (i + 1 < linked_messages.size() ? ", " : "");
			out << "]";
		}
		out << "\n";
	}

	void CodeComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "CodeComponent(" << content << ")";
		if (!pointer_messages.empty()) {
			out << " [pointers: ";
			for (size_t i = 0; i < pointer_messages.size(); ++i)
				out << pointer_messages[i] << (i + 1 < pointer_messages.size() ? ", " : "");
			out << "]";
		}
		if (!attached_messages.empty()) {
			out << " [attached: ";
			for (size_t i = 0; i < attached_messages.size(); ++i)
				out << attached_messages[i] << (i + 1 < attached_messages.size() ? ", " : "");
			out << "]";
		}
		out << "\n";
	}

	void ConcatComponent::reset() {
		for (auto& comp: components) comp->reset();
	}

	void ConcatComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "ConcatComponent:\n";
		for (const auto& comp: components) comp->debugPrint(out, indent + 1);
	}

	InteractiveComponent::InteractiveComponent(
		ComponentID id, Box<Component> primary, Box<Component> alternative
	):
		  Component(),
		  id(id),
		  primary(std::move(primary)),
		  alternative(std::move(alternative)) {
		this->primary->setParent(this);
		this->alternative->setParent(this);
	}

	void InteractiveComponent::reset() {
		status = Status::Primary;
		this->primary->reset();
		this->alternative->reset();
	}

	void InteractiveComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "InteractiveComponent:\n";
		out << std::string((indent + 1) * 2, ' ') << "Primary:\n";
		primary->debugPrint(out, indent + 2);
		out << std::string((indent + 1) * 2, ' ') << "Alternative:\n";
		alternative->debugPrint(out, indent + 2);
	}

	void StartLineComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "StartLineComponent("
			<< (number ? std::to_string(*number) : "none") << ")\n";
	}

	void CodeBlockComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "CodeBlockComponent:\n";
		content->debugPrint(out, indent + 1);
	}

	void CodeLocationComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "CodeLocationComponent(" << location.file << ":"
			<< location.line << ":" << location.column << ")\n";
	}

	void Diagnostic::debugPrint(std::ostream& out) const {
		out << "Diagnostic with " << messages.size() << " messages.\n";
		for (const auto& msg: messages) msg.debugPrint(out);
	}

	TextComponent::TextComponent(std::string content): Component(), content(std::move(content)) {}

	TextComponent::TextComponent(std::string content, std::vector<MessageID> attached_messages):
		  TextComponent(std::move(content)) {
		this->linked_messages = std::move(attached_messages);
	}

	CodeComponent::CodeComponent(
		ComponentID /* id */,
		const std::string&            content,
		std::vector<PointerMessageID> pointer_messages,
		std::vector<MessageID>        attached_messages
	):
		  Component(),
		  content(expandTabs(content)),
		  pointer_messages(std::move(pointer_messages)),
		  attached_messages(std::move(attached_messages)) {}

	CodeLocationComponent::CodeLocationComponent(CodeLocation location):
		  Component(),
		  location(std::move(location)) {}

	CodeBlockComponent::CodeBlockComponent(
		Box<Component> content, base::Optional<CodeLocation> location
	):
		  Component(),
		  content(std::move(content)),
		  location(std::move(location)) {
		this->content->setParent(this);
	}

	StartLineComponent::StartLineComponent(base::Optional<u64> number):
		  Component(),
		  number(number) {}

	ConcatComponent::ConcatComponent(std::vector<Box<Component>> components):
		  Component(),
		  components(std::move(components)) {
		for (auto& comp: this->components) comp->setParent(this);
	}

	Component::Component(CRef<Component> parent): parent(parent) {}

	void CodeLocationComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitCodeLocationComponent(*this);
	}

	void CodeBlockComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitCodeBlockComponent(*this);
	}

	void StartLineComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitStartLineComponent(*this);
	}

	void InteractiveComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitInteractiveComponent(*this);
	}

	void ConcatComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitConcatComponent(*this);
	}

	void CodeComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitCodeComponent(*this);
	}

	void TextComponent::acceptVisitor(ComponentVisitor& visitor) const {
		visitor.visitTextComponent(*this);
	}

	Message::Message(
		template_file::Metadata                         metadata,
		Box<Component>                                  header,
		MBox<Component>                                 description,
		base::HashMap<PointerMessageID, PointerMessage> pointer_messages,
		std::vector<ExploreEdge>                        explore_links
	):
		  metadata(std::move(metadata)),
		  header(std::move(header)),
		  description(std::move(description)),
		  pointer_messages(std::move(pointer_messages)),
		  explore_links(std::move(explore_links)) {}

	void Message::debugPrint(std::ostream& out) const {
		out << "Message: " << metadata.name << "\n";
		out << "Header: ";
		header->debugPrint(out);
		out << "\n";
		if (description) {
			out << "Description: ";
			description->debugPrint(out);
			out << "\n";
		}
		if (!pointer_messages.empty()) {
			out << "Pointer Messages:\n";
			for (const auto& [id, pm]: pointer_messages) {
				out << "  ID " << id << ": [" << pm.type << "] " << pm.content
					<< " (prio: " << pm.priority << ")\n";
			}
		}
		if (!explore_links.empty()) {
			out << "Explore Links:\n";
			for (const auto& edge: explore_links) {
				out << "  Link '" << edge.name << "': ";
				edge.content->debugPrint(out, 2);
			}
		}
	}

	namespace {
		/**
		 * @brief Visitor that collects MessageIDs from attached_messages in traversal order.
		 */
		class MessageLinkCollector: public ComponentVisitor {
			std::vector<MessageID>&       result;
			std::unordered_set<MessageID> seen;

			void addIfNew(MessageID id) {
				if (seen.insert(id).second) result.push_back(id);
			}

			void collectFromAttached(const std::vector<MessageID>& attached) {
				for (auto id: attached) addIfNew(id);
			}

		public:
			explicit MessageLinkCollector(std::vector<MessageID>& out): result(out) {}

			void visitTextComponent(const TextComponent& c) override {
				collectFromAttached(c.linked_messages);
			}

			void visitCodeComponent(const CodeComponent& c) override {
				collectFromAttached(c.attached_messages);
			}

			void visitConcatComponent(const ConcatComponent& c) override {
				for (const auto& child: c.components) child->acceptVisitor(*this);
			}

			void visitInteractiveComponent(const InteractiveComponent& c) override {
				c.primary->acceptVisitor(*this);
				c.alternative->acceptVisitor(*this);
			}

			void visitStartLineComponent(const StartLineComponent&) override {}

			void visitCodeBlockComponent(const CodeBlockComponent& c) override {
				c.content->acceptVisitor(*this);
			}

			void visitCodeLocationComponent(const CodeLocationComponent&) override {}
		};
	}  // namespace

	std::vector<MessageID> Message::getOrderedMessageLinks() const {
		std::vector<MessageID> result;
		MessageLinkCollector   collector(result);

		// 1. Header
		header->acceptVisitor(collector);

		// 2. Description
		if (description) description->acceptVisitor(collector);

		// 3. Explore links, in the order they were added by the compiler
		for (const auto& edge: explore_links) edge.content->acceptVisitor(collector);

		return result;
	}
}
