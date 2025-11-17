#pragma once

#include "common.hpp"
#include "interactive_code.hpp"
#include "serializable.hpp"

#include <helios/scope_symbol_id.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include "base/optional.hpp"
#include <base/box.hpp>

#include "diagnostic/source_position.hpp"

#include <json/json.hpp>

#include <string_view>
#include <utility>

namespace dia {
	enum class ContentType { ERROR, WARNING, NOTE, DOCS };

	NLOHMANN_JSON_SERIALIZE_ENUM(
		ContentType,
		{ { ContentType::ERROR, "error" },
	      { ContentType::WARNING, "warning" },
	      { ContentType::NOTE, "note" },
	      { ContentType::DOCS, "docs" } }
	)

	using nlohmann::json;

	/*
	 * Part of the InteractiveContent. Child classes are meant to gather the information about the
	 * error and output them in form of error template parameters.
	 */
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
		json tojson() override { return json::object(); }
	};

	/*
	 * Class representing actual individual error message. Idenifies error template that should be
	 * used by error family and name. Contains of ContentParams and code sample.
	 *
	 * Also responsible to gather all of the symbols and types contained in the message.
	 */
	class InteractiveContent {
	private:
		const ContentType                       content_type;
		const std::string                       family;
		const std::string                       name;
		Box<ContentParams>                      params;
		const base::Optional<Box<AbstractCode>> code_sample;

	public:
		InteractiveContent(
			ContentType                       content_type,
			std::string                       family,
			std::string                       name,
			Box<ContentParams>                params,
			base::Optional<Box<AbstractCode>> code_sample = {}
		):
			  content_type(content_type),
			  family(std::move(family)),
			  name(std::move(name)),
			  params(std::move(params)),
			  code_sample(std::move(code_sample)) {}

		json tojson() {
			return json{ { "metadata",
				           { { "type", content_type }, { "family", family }, { "name", name } } },
				         { "params", params },
				         { "code", code_sample } };
		}

		std::set<compiler::helios::SymID> getSymbols() {
			auto params_symbols = params->get_symbols();
			if_opt_some(code_sample, val) {
				auto code_symbols = val->get_symbols();
				params_symbols.insert(code_symbols.begin(), code_symbols.end());
			}
			return params_symbols;
		}

		std::set<tsh::AbstractType> getTypes() {
			auto params_types = params->get_types();
			if_opt_some(code_sample, val) {
				auto code_types = val->get_types();
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
	public:
		InteractiveNote(
			const std::string& family, const std::string& name, Box<ContentParams> params
		):
			  InteractiveContent(ContentType::NOTE, family, name, std::move(params)) {}

		InteractiveNote(
			const std::string& family,
			const std::string& name,
			Box<ContentParams> params,
			Box<AbstractCode>  code_sample
		):
			  InteractiveContent(
				  ContentType::NOTE, family, name, std::move(params), std::move(code_sample)
			  ) {}

		bool is_displayed() const { return is_default_displayed; }

	private:
		const bool is_default_displayed = true;
	};
}
