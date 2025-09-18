#include "../../hierarchy/declarations/top_level.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<TopLevel> TopLevel::parse(LangParserState& state) {
		auto out = makeBox<TopLevel>(state.getPosition());
		while (state.notEmpty()) {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (stmt) {
				out->statements.emplace_back(nullptr);
				state.parse(out).assign(&out->statements.back(), std::move(stmt));
			}
		}
		out->fillSymbols();

		return out;
	}

	void TopLevel::fillSymbols() {
		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				no_symbol.push_back(stmt.give());
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbolName().value();
				if (!by_symbol.atMaybe(symbol)) by_symbol.put(symbol);
				by_symbol[symbol].push_back(stmt.give());
				break;
			case DeclKind::Transparent:
				transparent.push_back(stmt.give());
				break;
			}
		}
	}

	void TopLevel::calcElementPathsRecursive() {
		calcOrderedListChildPath(statements, getElementPath());
	}

	void TopLevel::acceptVisitor(PstVisitor&) const { CORE_PANIC("Visitng TopLevel statement"); }

	void TopLevel::dprint(std::ostream& out) const {
		// @TODO: PST?
		out << "[";
		for (auto& e: statements) {
			nullAwareDprint(e, out);
			out << ", ";
		}
		out << "]";
	}

	LangElement::HashAlg& TopLevel::calcStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, statements.size());
		std::vector<std::pair<std::string, usize>> symbols_available_data;
		for(auto& [name, vec]: by_symbol) {
			symbols_available_data.emplace_back(name.str(), vec.size());	
		}
		std::ranges::sort(symbols_available_data);
		addToHash(partial_hash, symbols_available_data);
		return partial_hash;
	}
}
