#pragma once

#include "common.hpp"
#include "diagnostic/source_position.hpp"
#include "interactive_code.hpp"
#include "serializable.hpp"
#include "typesystem/higher/abstract_type.hpp"

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <json/json.hpp>

#include "base/optional.hpp"
#include <base/box.hpp>

#include <string_view>
#include <utility>

namespace dia {
	enum class ContentType { ERROR, WARNING, NOTE };

	NLOHMANN_JSON_SERIALIZE_ENUM(
		ContentType,
		{ { ContentType::ERROR, "error" },
	      { ContentType::WARNING, "warning" },
	      { ContentType::NOTE, "note" } }
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

	class EmptyParams: public ContentParams {
	public:
		json tojson() override { return json{}; }
	};

	class InteractiveContent {
	private:
		const ContentType                     content_type;
		const std::string                     family;
		const std::string                     name;
		Box<ContentParams>                    params;
		const base::Optional<InteractiveCode> interactive_code;

	public:
		InteractiveContent(
			ContentType                   content_type,
			const std::string&            family,
			const std::string&            name,
			Box<ContentParams>            params,
			dia::SourcePosition           position,
			pst::Access<pst::LangElement> pst,
			query::Context&               ctx,
			dia::pointer_message          pointer
		):
			  content_type(content_type),
			  family(family),
			  name(name),
			  params(std::move(params)),
			  interactive_code(InteractiveCode{ position, pst, ctx, pointer }) {}

		InteractiveContent(
			ContentType        content_type,
			const std::string& family,
			const std::string& name,
			Box<ContentParams> params
		):
			  content_type(content_type),
			  family(family),
			  name(name),
			  params(std::move(params)),
			  interactive_code() {}

		InteractiveContent(
			ContentType        content_type,
			const std::string& family,
			const std::string& name,
			Box<ContentParams> params,
			InteractiveCode    interactive_code
		):
			  content_type(content_type),
			  family(family),
			  name(name),
			  params(std::move(params)),
			  interactive_code(std::move(interactive_code)) {}

		json tojson() {
			return json{ { "metadata",
				           { { "type", content_type }, { "family", family }, { "name", name } } },
				         { "params", params },
				         { "code", { { "type", "code" }, { "content", interactive_code } } } };
		}

		std::set<compiler::helios::SymID> get_symbols() {
			auto params_symbols = params->get_symbols();
			if_opt_some(interactive_code, val) {
				auto code_symbols = val.get_symbols();
				params_symbols.insert(code_symbols.begin(), code_symbols.end());
			}
			return params_symbols;
		}

		std::set<tsh::AbstractType> get_types() {
			auto params_types = params->get_types();
			if_opt_some(interactive_code, val) {
				auto code_types = val.get_types();
				params_types.insert(code_types.begin(), code_types.end());
			}
			return params_types;
		}
	};

	class ExampleContent: public InteractiveContent {
	public:
		ExampleContent():
			  InteractiveContent(
				  ContentType::ERROR, "example", "examplename", base::makeBox<EmptyParams>()
			  ) {}
	};

	class InteractiveNote: public InteractiveContent {
		InteractiveNote(
			const std::string& family,
			const std::string& name,
			Box<ContentParams> params,
			InteractiveCode&&  interactive_code
		):
			  InteractiveContent(
				  ContentType::NOTE,
				  family,
				  name,
				  std::move(params),
				  std::forward<InteractiveCode>(interactive_code)
			  ) {}
	};
}
