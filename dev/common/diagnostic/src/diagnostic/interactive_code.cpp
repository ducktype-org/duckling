#include "interactive_code.hpp"
#include "base/optional.hpp"
#include "base/ref.hpp"
#include "interactive_logger.hpp"
#include "printer/stream_printer.hpp"
#include "pst_parser/access.hpp"
#include "pst_parser/lang_parser_element.hpp"
#include "query_framework/query_int.hpp"

using nlohmann::json;

void visit_leafs(CRef<pst::LangElement> pst, std::vector<json>& out, usize& last) {
	std::cout << pst->elementType() << std::endl;
	if (pst->viewChildren().empty()) {
		if (pst->getSourcePosition().getStart() > last + 1) {
			out.push_back(pst->getSourcePosition()
			                  .getSource()
			                  ->getCharRange(last + 1, pst->getSourcePosition().getStart())
			                  .stdString());
		}
		out.push_back({ { "name",
		                  pst->getSourcePosition()
		                      .getSource()
		                      ->getCharRange(
								  pst->getSourcePosition().getStart(),
								  pst->getSourcePosition().getEnd() + 1
							  )
		                      .stdString() },
		                { "type", pst->elementType() } });
		last = pst->getSourcePosition().getEnd();
		return;
	}

	for (auto c: pst->viewChildren()) visit_leafs(c, out, last);
}

json dia::InteractiveCode::serialize_code() const {
	std::cout << "called!" << std::endl;
	std::vector<json> fragments;
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
		auto parent_opt = parent->getParent().unlockOpt(ctx);
		if_opt_some(parent_opt, p) { parent = parent->getParent().unlock(ctx); }
		if_opt_none(parent_opt) { break; }
		range_start = parent->getSourcePosition().getStartLineColumn().first;
		range_end   = parent->getSourcePosition().getEndLineColumn().first;
	}
	CRef<pst::LangElement> p{ &*parent };
	usize                  last = parent->getSourcePosition().getStart();
	visit_leafs(p, fragments, last);
	return fragments;
}
