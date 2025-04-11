#pragma once

#include <json/json.hpp>
#include <helios/symbols/symbols.hpp>
#include <base/box.hpp>
#include "diagnostic/source_position.hpp"
#include "interactive_code.hpp"
#include "serializable.hpp"
#include "typesystem/higher/abstract_type.hpp"

namespace dia {
	enum ContentType { ERROR, WARNING, NOTE };

	NLOHMANN_JSON_SERIALIZE_ENUM(
		ContentType, { { ERROR, "error" }, { WARNING, "warning" }, { NOTE, "note" } }
	)

	using nlohmann::json;

	class ContentParams {
	protected:
		std::set<compiler::helios::SymID> symbols{};
		std::set<tsh::AbstractType>       types{};
		ContentParams() = default;

	public:
		std::set<compiler::helios::SymID> get_symbols() { return symbols; }

		std::set<tsh::AbstractType> get_types() { return types; }

		virtual json tojson()    = 0;
		virtual ~ContentParams() = default;
	};

	class EmptyParams: public virtual ContentParams {
	public:
		json tojson() override { return json{}; }
	};

	class InteractiveContent {
	private:
		const ContentType     content_type;
		const uint64_t        message_code;  // TODO: Probably soon to be replaced by string.
		Box<ContentParams>    params;
		const InteractiveCode interactive_code;

	public:
		InteractiveContent(
			ContentType         content_type,
			uint64_t            message_code,
			Box<ContentParams>  params,
			dia::SourcePosition position
		):
			  content_type(content_type),
			  message_code(message_code),
			  params(std::move(params)),
			  interactive_code(InteractiveCode{ position }) {}

		InteractiveContent(
			ContentType        content_type,
			uint64_t           message_code,
			Box<ContentParams> params,
			InteractiveCode    interactive_code
		):
			  content_type(content_type),
			  message_code(message_code),
			  params(std::move(params)),
			  interactive_code(interactive_code) {}

		json tojson() {
			return json{ { "type", content_type },  // TODO: Don't include this field in notes.
				         { "message_code", message_code },
				         { "params", params },
				         { "code", interactive_code } };
		}

		std::set<compiler::helios::SymID> get_symbols() {
			auto params_symbols = params->get_symbols();
			auto code_symbols   = interactive_code.get_symbols();
			params_symbols.insert(code_symbols.begin(), code_symbols.end());
			return params_symbols;
		}

		std::set<tsh::AbstractType> get_types() {
			auto params_types = params->get_types();
			auto code_types   = interactive_code.get_types();
			params_types.insert(code_types.begin(), code_types.end());
			return params_types;
		}
	};

	class ExampleContent: public InteractiveContent {
	public:
		ExampleContent():
			  InteractiveContent(
				  ERROR, 123, base::makeBox<EmptyParams>(), SourcePosition::fakePosition()
			  ) {}
	};

	class InteractiveNote: public InteractiveContent {
		InteractiveNote(
			uint64_t message_code, Box<ContentParams> params, InteractiveCode&& interactive_code
		):
			  InteractiveContent(
				  NOTE,
				  message_code,
				  std::move(params),
				  std::forward<InteractiveCode>(interactive_code)
			  ) {}
	};
}
