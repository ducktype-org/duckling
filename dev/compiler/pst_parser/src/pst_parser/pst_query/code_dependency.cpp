#include "code_dependency.hpp"
#include "pst_access_side_input.hpp"
#include "../lang_parser_element.hpp"

#include <diagnostic/location.hpp>
#include <query_framework/context.hpp>

#include <ranges>
#include <set>

namespace pst {
	std::strong_ordering filePosOrder(dia::SourcePosition one, dia::SourcePosition other) {
		auto one_path = one.getLocation()->getSourceFile();
		auto other_path = other.getLocation()->getSourceFile();

		auto path_ord = one_path.absolutePath() <=> other_path.absolutePath();
		if (path_ord != std::strong_ordering::equal) return path_ord;

		auto start_ord = one.getStart() <=> other.getStart();
		if (start_ord != std::strong_ordering::equal) return start_ord;

		return one.getEnd() <=> other.getEnd();
	}

	class ltFilePos {
	public:
		bool operator()(dia::SourcePosition one, dia::SourcePosition other) const {
			return filePosOrder(one, other) == std::strong_ordering::less;
		}
	};

	class ltFileTok {
	public:
		bool operator()(CRef<lexer::Token> one, CRef<lexer::Token> other) const {
			return filePosOrder(one->getPosition(), other->getPosition()) == std::strong_ordering::less;
		}
	};

	static auto viewDependentTokens(query::Context& ctx, query::detail::NodeID id) {
		using namespace std::views;

		auto nodes = ctx.getGraph().getNodeDepsFiltered(id, detail::PSTAccessSideInput::getID());	

		static auto get_pst_node = [](query::detail::NodeID id) {
			return LangElement::getById(id.hash.val);
		};
		static auto get_tokens = [](AccessLocked<LangElement> el) {
			return el.illegalAccess().map([](auto el){ 
				return el->viewTokens(); 
			});
		};
		static auto file_location = [](CRef<lexer::Token> tok){ 
			return tok->getPosition().getLocationType() == dia::LocationType::FileLocationType; 
		};

		return nodes 
			| transform(get_pst_node) 
			| transform(get_tokens) 
			| filter([](auto opt){ return opt.has_value(); })
			| transform([](auto opt) { return opt.value(); })
			| std::views::join
			| filter(file_location);
	}

	std::vector<dia::SourcePosition> queryPositionDependencies(query::Context& ctx, query::detail::NodeID id) {
		using namespace std::views;

		static auto get_token_pos = [](CRef<tpc::Token> tok) {
			return tok->getPosition();
		};

		auto x = viewDependentTokens(ctx, id)
			| transform(get_token_pos);
		
		std::set<dia::SourcePosition, ltFilePos> positions;
		for(auto el: x) positions.insert(el);

		if (positions.size() == 0) return {};
		dia::SourcePosition cur = *positions.begin();
		std::vector<dia::SourcePosition> merged_positions;
		for(auto pos: positions | drop(1)) {
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

	std::vector<CRef<lexer::Token>> queryTokenDependencies(query::Context& ctx, query::detail::NodeID id) {
		using namespace std::views;

		auto x = viewDependentTokens(ctx, id);
		
		std::set<CRef<lexer::Token>, ltFilePos> positions;
		for(auto el: x) positions.insert(el);

		return { positions.begin(), positions.end() };
	}
}