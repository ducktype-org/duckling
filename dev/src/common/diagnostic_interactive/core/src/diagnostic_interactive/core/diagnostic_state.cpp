#include "diagnostic_state.hpp"

namespace dia_app::state {
	void TextComponent::debugPrint(std::ostream& out, usize indent) const {
		out << std::string(indent * 2, ' ') << "TextComponent(" << content << ")";
		if (!attached_messages.empty()) {
			out << " [attached: ";
			for (size_t i = 0; i < attached_messages.size(); ++i)
				out << attached_messages[i] << (i + 1 < attached_messages.size() ? ", " : "");
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
	}

	void Diagnostic::debugPrint(std::ostream& out) const {
		out << "Diagnostic with " << messages.size() << " messages.\n";
		for (const auto& msg: messages) msg.debugPrint(out);
	}

	Message::Message(
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

	CodeComponent::CodeComponent(
		ComponentID /* id */,
		std::string                   content,
		std::vector<PointerMessageID> pointer_messages,
		std::vector<MessageID>        attached_messages
	):
		  Component(),
		  content(std::move(content)),
		  pointer_messages(std::move(pointer_messages)),
		  attached_messages(std::move(attached_messages)) {}

	TextComponent::TextComponent(std::string content, std::vector<MessageID> attached_messages):
		  Component(),
		  content(std::move(content)),
		  attached_messages(std::move(attached_messages)) {}

	TextComponent::TextComponent(std::string content): Component(), content(std::move(content)) {}

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
}
