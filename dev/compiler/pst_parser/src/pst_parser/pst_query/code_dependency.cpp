#include "code_dependency.hpp"

#include "../lang_parser_element.hpp"
#include "pst_access_side_input.hpp"

#include <diagnostic/location.hpp>
#include <query_framework/context.hpp>

#include <ranges>
#include <set>

namespace pst {

	class ltFileTok {
	public:
		bool operator()(CRef<lexer::Token> one, CRef<lexer::Token> other) const {
			return one->getPosition() < other->getPosition();
		}
	};

	/**
	 * @brief Common function for getting a list of tokens that a query depends on.
	 */
	static auto viewDependentTokens(query::detail::NodeID id) {
		using namespace std::views;

		auto nodes = query::Context::getGraph().getNodeDepsFiltered(
			id, detail::PSTAccessSideInput::getID()
		);

		static auto get_pst_node
			= [](query::detail::NodeID lid) { return LangElement::getById(lid.hash.val); };
		static auto get_tokens = [](AccessLocked<LangElement> locked) {
			return locked.illegalAccess().map([](auto el) { return el->viewTokens(); });
		};
		static auto file_location = [](CRef<lexer::Token> tok) {
			return tok->getPosition().getLocationType() == dia::LocationType::FileLocationType;
		};

		return nodes | transform(get_pst_node) | transform(get_tokens)
		     | filter([](auto opt) { return opt.has_value(); })
		     | transform([](auto opt) { return opt.value(); }) | std::views::join
		     | filter(file_location) | std::ranges::to<std::vector<CRef<lexer::Token>>>();
	}

	std::vector<dia::SourcePosition> queryPositionDependencies(query::detail::NodeID id) {
		using namespace std::views;

		static auto get_token_pos = [](CRef<tpc::Token> tok) { return tok->getPosition(); };

		auto x = viewDependentTokens(id) | transform(get_token_pos);

		std::set<dia::SourcePosition> positions;
		for (auto el: x) positions.insert(el);

		if (positions.size() == 0) return {};
		dia::SourcePosition              cur = *positions.begin();
		std::vector<dia::SourcePosition> merged_positions;
		for (auto pos: positions | drop(1)) {
			if (cur.getLocation()->getSourceFile() != pos.getLocation()->getSourceFile()
			    || cur.getEnd() + 1 < pos.getStart()) {
				merged_positions.push_back(cur);
				cur = pos;
				continue;
			}
			if (pos.getEnd() <= cur.getEnd() || cur.isFileEnd()) continue;
			cur = dia::SourcePosition(cur, pos.getEnd());
		}
		merged_positions.push_back(cur);

		return merged_positions;
	}

	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::detail::NodeID id) {
		using namespace std::views;

		auto x = viewDependentTokens(id);

		std::set<CRef<lexer::Token>, ltFileTok> positions;
		for (auto el: x) positions.insert(el);

		return { positions.begin(), positions.end() };
	}
}
