#include "code_dependency.hpp"

#include "../lang_parser_element.hpp"
#include "pst_access_side_input.hpp"

#include <diagnostic/location.hpp>
#include <query_framework/context.hpp>

#include <ranges>
#include <set>

namespace pst {

	namespace {
		class TokenLtComparisonFunctor {
		public:
			bool operator()(CRef<lexer::Token> one, CRef<lexer::Token> other) const {
				return one->getPosition() < other->getPosition();
			}
		};

		/**
		 * @brief Common function for getting a list of tokens that a query depends on.
		 */
		auto viewDependentTokens(query::internal::NodeID id) {
			using namespace std::views;

			auto nodes = query::Context::getState().getGraph().getNodeDepsFiltered(
				id, internal::PSTAccessSideInput::getID()
			);

			// We can retrieve the LangElement ID from the NodeId hash, because
			// LangElement ID is used as the first element of the unstable hash value.
			constexpr auto get_pst_node = [](query::internal::NodeID lid) {
				return LangElement::getByID(lid.hash.val.data.at(0));
			};
			constexpr auto get_tokens = [](AccessLocked<LangElement> locked) {
				return locked.illegalAccess().map([](auto el) { return el->viewTokens(); });
			};
			constexpr auto file_location = [](CRef<lexer::Token> tok) -> bool {
				return tok->getPosition().getLocationType() == dia::LocationType::FileLocationType;
			};

			// @note #1323 change it to std::ranges::to
			auto view_output =  nodes | transform(get_pst_node) | transform(get_tokens)
			     | filter([](auto opt) { return opt.has_value(); })
			     | transform([](auto opt) { return opt.value(); }) | std::views::join
			     | filter(file_location);

			std::vector<CRef<lexer::Token>> output;
			for (auto tok: view_output) output.emplace_back(tok);

			return output;
		}
	}

	std::vector<dia::SourcePosition> queryPositionDependencies(query::internal::NodeID id) {

		constexpr auto get_token_pos = [](CRef<lexer::Token> tok) { return tok->getPosition(); };

		auto x = viewDependentTokens(id) | std::views::transform(get_token_pos);

		// Merge the overlapping/adjacent positions
		// Might be made into a separate function in the future.

		std::set<dia::SourcePosition> positions;
		for (auto el: x) positions.insert(el);

		if (positions.size() == 0) return {};
		dia::SourcePosition              cur = *positions.begin();
		std::vector<dia::SourcePosition> merged_positions;
		for (auto pos: positions | std::views::drop(1)) {
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

	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::internal::NodeID id) {
		using namespace std::views;

		auto x = viewDependentTokens(id);

		// Remove repeating tokens

		std::set<CRef<lexer::Token>, TokenLtComparisonFunctor> positions;
		for (auto el: x) positions.insert(el);

		return { positions.begin(), positions.end() };
	}
}
