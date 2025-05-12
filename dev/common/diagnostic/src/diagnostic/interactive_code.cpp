#include "interactive_code.hpp"

#include "helios/scope_symbol_id.hpp"
#include "interactive_logger.hpp"
#include "printer/stream_printer.hpp"
#include "pst_parser/access.hpp"
#include "pst_parser/lang_parser_element.hpp"
#include "query_framework/query_int.hpp"

#include <helios/queries.hpp>
#include <helios/query_hout_of_expr.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <query_framework/query_entry_point.hpp>
#include <token_source/source.hpp>

#include "base/optional.hpp"
#include "base/ref.hpp"

#include <ostream>

using nlohmann::json;

base::Optional<compiler::helios::SymID> dia::InteractiveCode::get_symbol(
	pst::Access<pst::LangElement> pst
) const {
	auto c = dynamic_cast<const pst::ExprElement*>(&*pst);
	if (c) {
		auto m    = MCRef<pst::ExprElement>(c);
		auto expr = ctx.query<compiler::helios::QueryHoutOfExpr>({ m });
		if (expr.hasValue()) {
			auto hid = compiler::helios::querySymIDOfExpr(ctx, expr.value().ref());
			if (hid.has_value()) return hid.value();
		}
	}
	return {};
}

void dia::InteractiveCode::visit_leafs(pst::Access<pst::LangElement> pst, json& out) const {
	auto source = pst->getSourcePosition().getSource();
	// If this node is a leaf:
	if (pst->viewChildren().empty()) {
		auto hid_opt = get_symbol(pst);
		if (hid_opt.has_value()) {
			out.push_back({ { "content",
			                  pst->getSourcePosition()
			                      .getSource()
			                      ->getCharRange(
									  pst->getSourcePosition().getStart(),
									  pst->getSourcePosition().getEnd() + 1
								  )
			                      .stdString() },
			                { "type", "entity" },
			                { "refers_to", hid_opt.value().customPerfectHash() } });
			// TODO: add it to symbol list.
		} else {
			out.push_back({ { "content",
			                  pst->getSourcePosition()
			                      .getSource()
			                      ->getCharRange(
									  pst->getSourcePosition().getStart(),
									  pst->getSourcePosition().getEnd() + 1
								  )
			                      .stdString() },
			                { "type", "text" } });
		}

		return;
	}

	auto last_position = pst->getSourcePosition().getStart();

	// Not working because of query cycles.
	// auto hid_opt = get_symbol(pst);
	base::Optional<compiler::helios::SymID> hid_opt = {};
	for (auto c: pst->viewChildren()) {
		auto child       = c.unlock(ctx);
		auto child_start = child->getSourcePosition().getStart();
		auto child_end   = child->getSourcePosition().getEnd();
		if (child_start > last_position) {
			if (hid_opt.has_value()) {
				out.push_back({ { "type", "entity" },
				                { "content",
				                  source->getCharRange(last_position, child_start).stdString() },
				                { "refers_to", hid_opt.value().customPerfectHash() } });

			} else {
				out.push_back({ { "type", "text" },
				                { "content",
				                  source->getCharRange(last_position, child_start).stdString() } });
			}
		}
		json child_json{};
		child_json["content"] = json::array();
		child_json["type"]    = "grouping";
		visit_leafs(c.unlock(ctx), child_json["content"]);
		last_position = child_end + 1;
		out.push_back(child_json);
	}
	if (pst->getSourcePosition().getEnd() > last_position) {
		if (hid_opt.has_value()) {
			out.push_back({ { "type", "entity" },
			                { "content",
			                  source->getCharRange(last_position, pst->getSourcePosition().getEnd())
			                      .stdString() },
			                { "refers_to", hid_opt.value().customPerfectHash() } });

		} else {
			out.push_back({ { "type", "text" },
			                { "content",
			                  source->getCharRange(last_position, pst->getSourcePosition().getEnd())
			                      .stdString() } });
		}
	}
}

json dia::InteractiveCode::serialize_code() const {
	json fragments = json::array();
	// In the future we might want to get the code range based on its content, not just
	// expand by fixed amound of lines.
	auto parent = pst;
	auto start
		= position.getStartLineColumn().first > dia::InteractiveLogger::params().default_code_lines
	        ? position.getStartLineColumn().first
	              - dia::InteractiveLogger::params().default_code_lines
	        : 1;
	auto end
		= position.getEndLineColumn().first + dia::InteractiveLogger::params().default_code_lines;
	auto range_start = parent->getSourcePosition().getStartLineColumn().first;
	auto range_end   = parent->getSourcePosition().getEndLineColumn().first;
	while ((range_start != 1 && range_start >= start)
	       || (!parent->getSourcePosition().isFileEnd() && range_end <= end)) {
		auto parent_opt = parent->getParent();
		if_opt_some(parent_opt, p) { parent = p.unlock(ctx); }
		if_opt_none(parent_opt) { break; }
		range_start = parent->getSourcePosition().getStartLineColumn().first;
		range_end   = parent->getSourcePosition().getEndLineColumn().first;
	}
	visit_leafs(parent, fragments);
	return fragments;
}
