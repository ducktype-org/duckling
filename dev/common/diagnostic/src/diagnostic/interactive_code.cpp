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
#include <vector>

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

json dia::InteractiveCode::make_string_array(usize start, usize end) const {
	auto              lines = pst->getSourcePosition().getSource()->viewSplitRange(start, end);
	std::vector<json> v;
	for (auto& l: lines) {
		v.push_back({ { "type", "code" }, { "content", l.second.stdString() } });
		v.push_back({ { "type", "start_line" }, { "number", l.first + 1 } });
	}
	if (!v.empty()) v.pop_back();
	return v;
}

void dia::InteractiveCode::visit_leafs(pst::Access<pst::LangElement> pst, json& out) const {
	auto source = pst->getSourcePosition().getSource();
	// If this node is a leaf:
	if (pst->viewChildren().empty()) {
		auto hid_opt    = get_symbol(pst);
		auto node_start = pst->getSourcePosition().getStart();
		auto node_end   = pst->getSourcePosition().getEnd();
		json node       = { { "testcontent",
			                  pst->getSourcePosition()
			                      .getSource()
			                      ->getCharRange(node_start, node_end + 1)
			                      .stdString() } };
		node["content"] = make_string_array(node_start, node_end + 1);
		if (hid_opt.has_value()) {
			node["type"]      = "entity";
			node["refers_to"] = std::to_string(hid_opt.value().customPerfectHash());
			symbols.insert(hid_opt.value());
		} else {
			node["type"] = "grouping";
		}

		if (pointer.second.getStart() <= node_start && pointer.second.getEnd() >= node_end)
			node["groups"] = { pointer.first };
		out.push_back(node);
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
			json node       = { { "testcontent",
				                  source->getCharRange(last_position, child_start).stdString() } };
			node["content"] = make_string_array(last_position, child_start);
			if (hid_opt.has_value()) {
				node["type"]      = "entity";
				node["refers_to"] = std::to_string(hid_opt.value().customPerfectHash());
				symbols.insert(hid_opt.value());
			} else {
				node["type"] = "grouping";
			}
			if (pointer.second.getStart() <= last_position && pointer.second.getEnd() >= child_start)
				node["groups"] = { pointer.first };
			out.push_back(node);
		}
		json child_json{};
		child_json["content"] = json::array();
		child_json["type"]    = "grouping";
		visit_leafs(c.unlock(ctx), child_json["content"]);
		last_position = child_end + 1;
		out.push_back(child_json);
	}
	if (pst->getSourcePosition().getEnd() > last_position) {
		json node = {
			{ "testcontent",
			  source->getCharRange(last_position, pst->getSourcePosition().getEnd()).stdString() }
		};
		node["content"] = make_string_array(last_position, pst->getSourcePosition().getEnd());
		if (hid_opt.has_value()) {
			node["type"]      = "entity";
			node["refers_to"] = std::to_string(hid_opt.value().customPerfectHash());
			symbols.insert(hid_opt.value());
		} else {
			node["type"] = "grouping";
		}
		out.push_back(node);
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
	fragments.push_back({ { "type", "start_line" },
	                      { "number", parent->getSourcePosition().getStartLineColumn().first } });
	visit_leafs(parent, fragments);
	return fragments;
}
