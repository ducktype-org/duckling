#pragma once

#include "common.hpp"
#include "interactive_content.hpp"

#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>

#include "base/string_id.hpp"
#include <base/box.hpp>

#include "diagnostic/interactive_code.hpp"
#include "query_framework/query_int.hpp"
#include <helios/scope_symbol_id.hpp>

#include <json/json.hpp>

#include <map>
#include <vector>

namespace dia {
	using nlohmann::json;
	/* This is the main error class. If you want to create a new error, you need to inherit from this class.
	 * It composes of the main content and side notes providing additional information.
	 *
	 * Check out compiler/code/helios/errors/operator_not_found to see how to create a custom error from scrach.
	*/
	class InteractiveMessage {
	private:
		Box<dia::InteractiveContent>                     content;
		std::map<std::string, Box<dia::InteractiveNote>> notes;

	public:
		InteractiveMessage(
			Box<dia::InteractiveContent>                       content,
			std::map<std::string, Box<dia::InteractiveNote>>&& notes
		):
			  content(std::move(content)),
			  notes(std::move(notes)) {}

		friend void to_json(json& j, const InteractiveMessage& message) { j = message.tojson(); }

		json tojson() const;
	};

	class ExampleMessage: public InteractiveMessage {
	public:
		ExampleMessage():
			  InteractiveMessage(
				  base::makeBox<ExampleContent>(), std::map<std::string, Box<dia::InteractiveNote>>()
			  ) {}
	};

	class ParseError: public dia::InteractiveMessage {
	public:
		ParseError(std::string family, std::string name, dia::SourcePosition position):
			  dia::InteractiveMessage(
				  makeBox<InteractiveContent>(
					  ContentType::ERROR,
					  family,
					  name,
					  base::makeBox<EmptyParams>(),
					  base::makeBox<SimpleCode>(position, pointer_message{ "cause", position })
				  ),
				  std::map<std::string, Box<dia::InteractiveNote>>{}
			  ) {}
	};

	class RoundBracket: public ParseError {
	public:
		RoundBracket(dia::SourcePosition position):
			  ParseError("parse", "for_round_bracket", position) {}
	};

	/*
	 * Placeholder class. You can use it to report errors while developing some functionality,
	 * with the intention to replace it with something custom later.
	 * 
	*/ 
	class TODOError: public InteractiveMessage {
		class Params: public ContentParams {
			const std::string message;

		public:
			Params(const std::string& message): message(message) {}

			json tojson() override { return { "message", message }; }
		};

	public:
		TODOError(dia::SourcePosition position, const std::string& message):
			  dia::InteractiveMessage(
				  makeBox<InteractiveContent>(
					  ContentType::ERROR,
					  "misc",
					  "todo",
					  base::makeBox<Params>(message),
					  base::makeBox<SimpleCode>(position, pointer_message{ "cause", position })
				  ),
				  std::map<std::string, Box<dia::InteractiveNote>>{}
			  ) {}
	};
}
