#pragma once

#include "common.hpp"
#include "pst_parser/lang_parser_element.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/lang_parser_element.hpp>
#include <pst_parser/pst.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <base/ref.hpp>

#include <diagnostic/source_position.hpp>

#include <json/json.hpp>

#include <set>
#include <string>
#include <utility>

namespace dia {
	using pointer_message = dia::pointer_message;
	using nlohmann::json;

	/*
	 * Responsible for creating a code sample.
	 * Available child classes:
	 * 	- InteractiveCode, which creates a code sample semanticly structured and enriched with
	 * compiler symbols. Needed for all of view manager features to be available.
	 * 	- SimpleCode, which just outputs the code as a block of text.
	 */
	class AbstractCode {
	protected:
		dia::SourcePosition                       position;
		mutable std::set<compiler::helios::SymID> symbols{};
		mutable std::set<tsh::AbstractType>       types{};
		pointer_message                           pointer;

		AbstractCode(dia::SourcePosition position, pointer_message pointer):
			  position(position),
			  pointer(pointer) {}

	public:
		virtual json serialize_code() const = 0;

		json tojson() {
			auto j        = json::object();
			j["location"] = position;
			j["content"]  = serialize_code();
			return j;
		}

		std::set<compiler::helios::SymID> get_symbols() const { return symbols; }

		std::set<tsh::AbstractType> get_types() const { return types; }

		virtual ~AbstractCode() = default;
	};

	class InteractiveCode: public AbstractCode {
	private:
		pst::AccessLocked<pst::LangElement> pst;
		query::Context&                     ctx;
		void                                visit_leafs(
										   pst::AccessLocked<pst::LangElement> pst, json& out, usize range_start, usize range_end
									   ) const;

	public:
		json serialize_code() const override;

		InteractiveCode(
			dia::SourcePosition                 position,
			pst::AccessLocked<pst::LangElement> pst,
			query::Context&                     ctx,
			pointer_message                     pointer
		):
			  AbstractCode(position, pointer),
			  pst(pst),
			  ctx(ctx) {}
	};

	// Non-interactive code. Might be useful in parse errors.
	class SimpleCode: public AbstractCode {
	public:
		json serialize_code() const override;

		SimpleCode(dia::SourcePosition position, pointer_message pointer):
			  AbstractCode(position, pointer) {}
	};
}
