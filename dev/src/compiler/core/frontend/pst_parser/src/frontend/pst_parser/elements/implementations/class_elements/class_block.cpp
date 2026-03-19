#include "../../hierarchy/not_statements/class_block.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<ClassBlock> ClassBlock::parse(LangParserState& state) {
		CORE_ASSERT(
			state.getContext()->block_order == BlockOrderType::Unordered,
			"Class block should have unordered order type"
		);

		auto out = makeBox<ClassBlock>(state);

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.logInt(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		PARSE().goDown();

		PST_WHILE(state.notEmpty()) {
			MBox<ClassStmt> stmt;
			PARSE().with(&stmt, ClassStmt::parse);
			if (stmt) {
				out->statements.emplace_back(nullptr);
				PARSE().assign(&out->statements.back(), std::move(stmt));
			}
		}

		PARSE().goUpAndSkip();

		out->fillSymbols();

		PST_RETURN out;
	}

	void ClassBlock::fillSymbols() {
		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				no_symbol.emplace_back(stmt.give());
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbolName().value();
				if (!by_symbol.atMaybe(symbol)) by_symbol.put(symbol);
				by_symbol[symbol].emplace_back(stmt.give());
				break;
			case DeclKind::Transparent:
				transparent.emplace_back(stmt.give());
				break;
			}
		}
	}

	void ClassBlock::calcElementPathHashRecursive() {
		auto                          path            = getElementPathHash();
		auto                          no_symbol_path  = hashing::ComponentHash(path, "no_symbol");
		usize                         no_symbol_count = 0;
		auto                          by_symbol_path  = hashing::ComponentHash(path, "by_symbol");
		base::Map<base::StrID, usize> by_symbol_count;
		usize                         symbol_count = 0;
		auto  transparent_path                     = hashing::ComponentHash(path, "transparent");
		usize transparent_count                    = 0;

		hashing::ComponentHash id_path = no_symbol_path;

		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				id_path
					= hashing::ComponentHash(no_symbol_path, std::format("[{}]", no_symbol_count));
				calcChildPath(stmt, id_path);
				no_symbol_count++;
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbolName().value();
				if (by_symbol_count.atMaybe(symbol)) {
					symbol_count = by_symbol_count[symbol];
				} else {
					by_symbol_count.put(symbol);
					symbol_count = 0;
				}
				id_path = hashing::ComponentHash(
					by_symbol_path, std::format("{}[{}]", symbol.str(), symbol_count)
				);
				calcChildPath(stmt, id_path);
				by_symbol_count[symbol]++;
				break;
			case DeclKind::Transparent:
				id_path = hashing::ComponentHash(
					transparent_path, std::format("[{}]", transparent_count)
				);
				calcChildPath(stmt, id_path);
				transparent_count++;
				break;
			}
		}
	}

	void ClassBlock::dprint(std::ostream& out) const {
		out << "[";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]";
	}

	HashAlg& ClassBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, statements.size());
		addToHash(partial_hash, no_symbol.size());
		addToHash(partial_hash, transparent.size());
		std::vector<std::pair<std::string, usize>> symbols_available_data;
		for (auto& [name, vec]: by_symbol)
			symbols_available_data.emplace_back(name.strView(), vec.size());
		std::ranges::sort(symbols_available_data);
		addToHash(partial_hash, symbols_available_data);
		return partial_hash;
	}
}
