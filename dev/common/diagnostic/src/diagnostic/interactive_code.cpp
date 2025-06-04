#include "interactive_code.hpp"

#include "helios/scope_symbol_id.hpp"
#include "interactive_logger.hpp"
#include "printer/stream_printer.hpp"
#include "pst_parser/access.hpp"
#include "query_framework/query_int.hpp"

#include <helios/queries.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <pst_parser/lang_parser_element.hpp>
#include <query_framework/query_entry_point.hpp>
#include <token_source/source.hpp>

#include "base/exceptions.hpp"
#include "base/optional.hpp"
#include "base/ref.hpp"

#include <ostream>
#include <vector>

using nlohmann::json;

namespace {
	json make_string_array(Ref<tokenizer::TokenSource> source, usize start, usize end) {
		auto              lines = source->viewSplitRange(start, end);
		std::vector<json> v;
		for (auto& l: lines) {
			v.emplace_back(l.second.stdString());
			v.push_back({ { "type", "start_line" }, { "number", l.first + 1 } });
		}
		if (!v.empty()) v.pop_back();
		return v;
	}
}

void dia::InteractiveCode::visit_leafs(
	pst::AccessLocked<pst::LangElement> p, json& out, usize range_start, usize range_end
) const {
	auto pst    = p.unlock(ctx);
	auto source = pst->getSourcePosition().getSource();
	// If this node is a leaf:
	auto node_line = pst->getSourcePosition().getStartLineColumn().first;
	if (pst->viewChildren().empty() && node_line >= range_start && node_line <= range_end) {
		auto                                    pst_expr = pst.dynamicCast<pst::ExprElement>();
		base::Optional<compiler::helios::SymID> hid_opt;
		if (pst_expr) hid_opt = compiler::helios::querySymIDOfPSTExpr(ctx, pst_expr.value());
		auto node_start = pst->getSourcePosition().getStart();
		auto node_end   = pst->getSourcePosition().getEnd();
		json node       = json::object();
		node["content"]
			= make_string_array(pst->getSourcePosition().getSource(), node_start, node_end + 1);
		if (hid_opt.has_value()) {
			node["type"]      = "entity";
			node["refers_to"] = std::to_string(hid_opt.value().queryUnstablePerfectHash());
			symbols.insert(hid_opt.value());
		} else {
			node["type"] = "grouping";
		}

		if (pointer.second.getStart() <= node_start && pointer.second.getEnd() >= node_end)
			node["groups"] = { pointer.first };
		out.push_back(node);
		return;
	}

	auto                                    last_position = pst->getSourcePosition().getStart();
	base::Optional<compiler::helios::SymID> hid_opt       = {};
	auto                                    pst_expr      = pst.dynamicCast<pst::ExprElement>();
	try {
		// if (pst_expr) hid_opt = compiler::helios::querySymIDOfPSTExpr(ctx, pst_expr.value());
	} catch (base::NotYetImplemented&) {}  // I just want to catch cycles, but well...

	for (auto c: pst->viewChildren()) {
		auto child       = c.unlock(ctx);
		auto child_start = child->getSourcePosition().getStart();
		auto child_end   = child->getSourcePosition().getEnd();
		// TODO: check if this is in line range.
		if (child_start > last_position) {
			json node       = json::object();
			node["content"] = make_string_array(
				pst->getSourcePosition().getSource(), last_position, child_start
			);
			if (hid_opt.has_value()) {
				node["type"]      = "entity";
				node["refers_to"] = std::to_string(hid_opt.value().queryUnstablePerfectHash());
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
		visit_leafs(child, child_json["content"], range_start, range_end);
		last_position = child_end + 1;
		out.push_back(child_json);
	}
	// TODO: this is awful copypaste.
	if (pst->getSourcePosition().getEnd() > last_position) {
		json node       = json::object();
		node["content"] = make_string_array(
			pst->getSourcePosition().getSource(),
			last_position,
			pst->getSourcePosition().getEnd() + 1
		);
		if (hid_opt.has_value()) {
			node["type"]      = "entity";
			node["refers_to"] = std::to_string(hid_opt.value().queryUnstablePerfectHash());
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
	auto parent = pst.unlock(ctx);
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
		if (parent_opt)
			parent = parent_opt.value().unlock(ctx);
		else
			break;
		range_start = parent->getSourcePosition().getStartLineColumn().first;
		range_end   = parent->getSourcePosition().getEndLineColumn().first;
	}
	fragments.push_back({ { "type", "start_line" },
	                      { "number", parent->getSourcePosition().getStartLineColumn().first } });
	visit_leafs(parent, fragments, start, end);
	return fragments;
}

json dia::SimpleCode::serialize_code() const {
	auto source = position.getSource();

	usize start_line = position.getStartLineColumn().first;
	usize end_line   = position.getEndLineColumn().first;

	auto  code_lines = InteractiveLogger::params().default_code_lines;
	usize first_line = std::max(code_lines + 1, start_line) - code_lines;
	usize last_line  = std::min(source->getLines().size(), end_line + code_lines);

	usize begin_char = source->getLine(first_line).first;
	usize end_char   = source->getLine(last_line).second;

	auto j = json::array();
	j.push_back({ { "type", "start_line" }, { "number", first_line } });
	auto before = make_string_array(source, begin_char, pointer.second.getStart());
	auto points_to
		= make_string_array(source, pointer.second.getStart(), pointer.second.getEnd() + 1);
	j.insert(j.end(), before.begin(), before.end());
	j.push_back({ { "type", "grouping" }, { "content", points_to }, { "groups", { pointer.first } } }
	);
	auto after = make_string_array(source, pointer.second.getEnd() + 1, end_char + 1);
	j.insert(j.end(), after.begin(), after.end());

	return j;
}
