#include <diagnostic/core/diagnostic_state.hpp>
#include <diagnostic/core/term_ui_view.hpp>
#include <diagnostic/core/view_constructors.hpp>

namespace dia {

	/**
	 * @brief Convert state components to plain text.
	 * Used for evaluating patterns in CaseOfComponent.
	 */
	class ConstructTextViewVisitor: public state::ComponentVisitorPanicky {
	public:
		std::string result;

		void visitTextComponent(const state::TextComponent& el) override { result += el.content; }

		void visitCodeComponent(const state::CodeComponent& el) override { result += el.content; }

		void visitConcatComponent(const state::ConcatComponent& el) override {
			for (const auto& component: el.components) (*component).acceptVisitor(*this);
		}

		void visitInteractiveComponent(const state::InteractiveComponent& el) override {
			(*el.primary).acceptVisitor(*this);
		}

		void visitStartLineComponent(const state::StartLineComponent&) override { result += "\n"; }

		void visitCodeBlockComponent(const state::CodeBlockComponent& el) override {
			(*el.content).acceptVisitor(*this);
		}

		void visitCodeLocationComponent(const state::CodeLocationComponent&) override {}
	};

	std::string constructTextView(CRef<state::Component> component) {
		ConstructTextViewVisitor text_visitor;
		component->acceptVisitor(text_visitor);
		return text_visitor.result;
	}

	using namespace term_ui_view;

	StyleType styleTypeFromString(const std::string& type) {
		if (type == "error") return StyleType::Error;
		if (type == "warning") return StyleType::Warning;
		if (type == "note") return StyleType::Note;
		if (type == "hint") return StyleType::Hint;
		if (type == "docs") return StyleType::Docs;
		return StyleType::Error;
	}

	class CodeSectionBuilder: public state::ComponentVisitor {
		std::vector<CodeLine>& lines;
		std::vector<CodePiece> current_line_pieces;
		base::Optional<u64>    current_line_no;

	public:
		CodeSectionBuilder(std::vector<CodeLine>& lines): lines(lines) {}

		void flushLine() {
			if (!current_line_pieces.empty() || current_line_no.has_value()) {
				lines.emplace_back();
				lines.back().line_no = current_line_no;
				lines.back().pieces  = std::move(current_line_pieces);

				current_line_pieces.clear();
				current_line_no.reset();
			}
		}

		void visitStartLineComponent(const state::StartLineComponent& c) override {
			flushLine();
			current_line_no = c.number;
		}

		void visitCodeComponent(const state::CodeComponent& c) override {
			CodePiece piece;
			piece.text = c.content;
			for (auto id: c.pointer_messages) piece.pointer_ids.insert(id);
			current_line_pieces.push_back(std::move(piece));
		}

		void visitTextComponent(const state::TextComponent& c) override {
			CodePiece piece;
			piece.text = c.content;
			current_line_pieces.push_back(std::move(piece));
		}

		void visitConcatComponent(const state::ConcatComponent& c) override {
			for (auto& child: c.components) child->acceptVisitor(*this);
		}

		void visitInteractiveComponent(const state::InteractiveComponent& c) override {
			c.primary->acceptVisitor(*this);
		}

		void visitCodeBlockComponent(const state::CodeBlockComponent& c) override {
			c.content->acceptVisitor(*this);
		}

		void visitCodeLocationComponent(const state::CodeLocationComponent&) override {}
	};

	CodeSection buildCodeSection(
		const state::CodeBlockComponent&                                     block,
		const base::HashMap<state::PointerMessageID, state::PointerMessage>& pointer_msgs,
		StyleType                                                            style_type
	) {
		CodeSection section;
		if (block.location.has_value()) {
			section.location = block.location.value();
		} else {
			section.location.file   = "";
			section.location.line   = 0;
			section.location.column = 0;
		}

		CodeSectionBuilder builder(section.lines);
		block.content->acceptVisitor(builder);
		builder.flushLine();

		// Extract pointers used in lines
		for (const auto& line: section.lines) {
			for (const auto& piece: line.pieces) {
				for (auto id: piece.pointer_ids) {
					if (pointer_msgs.contains(id) && !section.pointers.contains(id)) {
						const auto&    msg = pointer_msgs.at(id);
						PointerMessage ptr_msg;
						ptr_msg.text     = msg.content;
						ptr_msg.priority = msg.priority;
						ptr_msg.type     = style_type;  // Inherit style from message
						section.pointers.put(id, std::move(ptr_msg));
					}
				}
			}
		}
		return section;
	}

	class MessageBuilder: public state::ComponentVisitor {
		std::vector<Section>&                                                sections;
		std::string                                                          current_text;
		const base::HashMap<state::PointerMessageID, state::PointerMessage>& pointer_msgs;
		StyleType                                                            style_type;

	public:
		MessageBuilder(
			std::vector<Section>&                                                sections,
			const base::HashMap<state::PointerMessageID, state::PointerMessage>& pointer_msgs,
			StyleType                                                            style_type
		):
			  sections(sections),
			  pointer_msgs(pointer_msgs),
			  style_type(style_type) {}

		void flushText() {
			if (!current_text.empty()) {
				sections.emplace_back(current_text);
				current_text.clear();
			}
		}

		void visitTextComponent(const state::TextComponent& c) override {
			current_text += c.content;
		}

		void visitCodeBlockComponent(const state::CodeBlockComponent& c) override {
			flushText();
			sections.emplace_back(buildCodeSection(c, pointer_msgs, style_type));
		}

		void visitConcatComponent(const state::ConcatComponent& c) override {
			for (auto& child: c.components) child->acceptVisitor(*this);
		}

		void visitInteractiveComponent(const state::InteractiveComponent& c) override {
			c.primary->acceptVisitor(*this);
		}

		void visitCodeComponent(const state::CodeComponent& c) override {
			current_text += c.content;
		}

		void visitStartLineComponent(const state::StartLineComponent&) override {
			current_text += "\n";
		}

		void visitCodeLocationComponent(const state::CodeLocationComponent&) override {}
	};

	term_ui_view::Diagnostic constructTreeView(const state::Diagnostic& state) {
		term_ui_view::Diagnostic diag;
		for (auto& msg_id: state.displayed_messages) {
			auto&                 msg = state.messages.at(msg_id);
			term_ui_view::Message view_msg;
			view_msg.type   = styleTypeFromString(msg.metadata.type);
			view_msg.code   = msg.metadata.code;
			view_msg.header = constructTextView(msg.header.ref());

			if (msg.description) {
				MessageBuilder builder(view_msg.sections, msg.pointer_messages, view_msg.type);
				msg.description->acceptVisitor(builder);
				builder.flushText();
			}
			if (not msg.explore_links.empty()) {
				std::string explore_links = "Explore more:\n";
				for (auto& edge: msg.explore_links) {
					auto test = constructTextView(edge.content.ref());
					explore_links += "* " + test + "\n";
				}
				view_msg.sections.emplace_back(TextSection{ explore_links });
			}
			diag.messages.push_back(std::move(view_msg));
		}
		return diag;
	}


}
