#include "component_pieces.hpp"

namespace term_ui {
	TextPieces::TextPieces(const view::NoHlComponent& component) { add_component(component); }

	std::string TextPieces::to_string() const {
		std::string res;
		for (auto& piece: pieces) res += piece;
		return res;
	}

	void TextPieces::add_component(const view::NoHlComponent& component) {
		// Associated entries are ignored in static UI.
		if (component.has_text_component()) {
			pieces.push_back(component.text_component().content());

		} else if (component.has_code_component()) {
			// Code is not distinguished from plain text in the terminal.
			pieces.push_back(component.code_component().content());

		} else if (component.has_interactive_component()) {
			add_component(component.interactive_component().primary_component());

		} else {  // component.has_concat_component()
			auto& concat = component.concat_component();
			for (u32 i = 0; i < concat.components_size(); ++i) add_component(concat.components(i));
		}
	}

	CodePiece::CodePiece(std::string text): text(text) {}

	CodePiece::CodePiece(std::string text, std::set<u32> groups): text(text), groups(groups) {}

	CodePiece::CodePiece(const view::HlCodeComponent& component) {
		text = component.content();
		for (u32 i = 0; i < component.hl_tags_size(); ++i) groups.insert(component.hl_tags(i));
	}

	const std::set<u32>& CodePiece::getGroups() const { return groups; }

	const std::string& CodePiece::getText() const { return text; }

	CodePieces::CodePieces(const view::HlComponent& component) { add_component(component); }

	const std::vector<CodePiece>& CodePieces::getPieces() const { return pieces; }

	std::string CodePieces::to_string() const {
		std::string res;
		for (auto& piece: pieces) res += piece.getText();
		return res;
	}

	void CodePieces::add_component(const view::HlComponent& component) {
		if (component.has_code_component()) {
			pieces.emplace_back(component.code_component());
		} else if (component.has_interactive_component()) {
			add_component(component.interactive_component().primary_component());
		} else {  // component.has_concat_component()
			auto& concat = component.concat_component();
			for (u32 i = 0; i < concat.components_size(); ++i) add_component(concat.components(i));
		}
	}
}
