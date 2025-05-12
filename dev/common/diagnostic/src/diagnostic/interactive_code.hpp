#pragma once

#include "pst_parser/lang_parser_element.hpp"
#include "source_position.hpp"

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <json/json.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <set>

namespace dia {
	using nlohmann::json;

	class InteractiveCode {
	private:
		dia::SourcePosition               position;
		std::set<compiler::helios::SymID> symbols;
		std::set<tsh::AbstractType>       types;
		pst::Access<pst::LangElement>     pst;
		query::Context&                   ctx;

		void visit_leafs(pst::Access<pst::LangElement> pst, std::vector<json>& out, usize& last)
			const;
		json serialize_code() const;

	public:
		InteractiveCode(
			dia::SourcePosition position, pst::Access<pst::LangElement> pst, query::Context& ctx
		):
			  position(position),
			  pst(pst),
			  ctx(ctx) {}

		friend void to_json(json& j, const InteractiveCode& code) {
			j = json{ { "location", code.position }, { "text", code.serialize_code() } };
		}

		std::set<compiler::helios::SymID> get_symbols() const { return symbols; }

		std::set<tsh::AbstractType> get_types() const { return types; }
	};
}
