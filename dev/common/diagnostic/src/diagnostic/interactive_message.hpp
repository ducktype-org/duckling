#pragma once

#include <json/json.hpp>
#include <base/box.hpp>
#include "serializable.hpp"
#include "interactive_content.hpp"
#include <vector>
#include <concepts>

namespace dia {
	using nlohmann::json;

	class InteractiveMessage {
	private:
		Box<dia::InteractiveContent>           content;
		std::vector<Box<dia::InteractiveNote>> notes;

	public:
		InteractiveMessage(
			Box<dia::InteractiveContent> content, std::vector<Box<dia::InteractiveNote>>&& notes
		):
			  content(std::move(content)),
			  notes(std::forward<decltype(notes)>(notes)) {}

		friend void to_json(json& j, const InteractiveMessage& message) {
			auto content_symbols = message.content->get_symbols();
			for (auto& note: message.notes) {
				auto note_symbols = note->get_symbols();
				content_symbols.insert(note_symbols.begin(), note_symbols.end());
			}
			j = json{ { "content", message.content },
				      { "notes", message.notes },
				      { "symbols", content_symbols } };
		}
	};

	class ExampleMessage: public InteractiveMessage {
	public:
		ExampleMessage():
			  InteractiveMessage(
				  base::makeBox<ExampleContent>(), std::vector<Box<dia::InteractiveNote>>()
			  ) {}
	};
}
