#pragma once

#include "common.hpp"
#include "pst_parser/lang_parser_element.hpp"
#include "source_position.hpp"

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <json/json.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <set>

namespace dia {
	using pointer_message = dia::pointer_message;
	using nlohmann::json;

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
			j["content"]  = { { "type", "code" }, { "content", serialize_code() } };
			return j;
		}

		std::set<compiler::helios::SymID> get_symbols() const { return symbols; }

		std::set<tsh::AbstractType> get_types() const { return types; }

		virtual ~AbstractCode() = default;
	};

	class InteractiveCode: public AbstractCode {
	private:
		pst::Access<pst::LangElement>           pst;
		query::Context&                         ctx;
		base::Optional<compiler::helios::SymID> get_symbol(pst::Access<pst::LangElement> pst) const;
		void                                    visit_leafs(
											   pst::Access<pst::LangElement> pst, json& out, usize range_start, usize range_end
										   ) const;

	public:
		json serialize_code() const override;

		InteractiveCode(
			dia::SourcePosition           position,
			pst::Access<pst::LangElement> pst,
			query::Context&               ctx,
			pointer_message               pointer
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
