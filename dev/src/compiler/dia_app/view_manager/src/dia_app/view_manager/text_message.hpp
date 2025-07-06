#pragma once
#include "common.hpp"
#include "component.hpp"

namespace dia {
	json create_text_component_json(const std::string& text) {
		return json{ { { METADATA, { TYPE, TEXT } }, { CONTENT, text } } };
	}

	class TextMessage: public Component {
	private:
		std::string text;

	public:
		TextMessage(std::string text): Component(std::vector<c_ptr>()), text(text) {}

		json to_json() const { return create_text_component_json(text); }
	};
}  // namespace dia
