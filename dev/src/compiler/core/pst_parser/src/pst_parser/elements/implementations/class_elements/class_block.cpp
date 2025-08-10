#include "../../hierarchy/not_statements/class_block.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<ClassBlock> ClassBlock::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<ClassBlock>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		while (state.notEmpty()) {
			MBox<ClassStmt> stmt;
			state.parse(out).with(&stmt, ClassStmt::parse, ctx);
			out->statements.emplace_back(nullptr);
			state.parse(out).assign(&out->statements.back(), std::move(stmt));
		}

		state.parse(out).goUpAndSkip();

		out->fillSymbols();

		return out;
	}

	void ClassBlock::fillSymbols() {
		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				no_symbol.emplace_back(stmt.give());
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbol().value();
				if (!by_symbol.atMaybe(symbol)) by_symbol.put(symbol);
				by_symbol[symbol].emplace_back(stmt.give());
				break;
			case DeclKind::Transparent:
				transparent.emplace_back(stmt.give());
				break;
			}
		}
	}

	void ClassBlock::calcElementPathsRecursive(const ElementPath& path) {
		auto                          no_symbol_path  = ElementPath(path, "no_symbol");
		usize                         no_symbol_count = 0;
		auto                          by_symbol_path  = ElementPath(path, "by_symbol");
		base::Map<base::StrID, usize> by_symbol_count;
		usize                         symbol_count      = 0;
		auto                          transparent_path  = ElementPath(path, "transparent");
		usize                         transparent_count = 0;

		ElementPath id_path = no_symbol_path;

		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				id_path = ElementPath(no_symbol_path, std::format("[{}]", no_symbol_count));
				calcChildPath(stmt, id_path);
				no_symbol_count++;
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbol().value();
				if (by_symbol_count.atMaybe(symbol)) {
					symbol_count = by_symbol_count[symbol];
				} else {
					by_symbol_count.put(symbol);
					symbol_count = 0;
				}
				id_path
					= ElementPath(by_symbol_path, std::format("{}[{}]", symbol.str(), symbol_count));
				calcChildPath(stmt, id_path);
				by_symbol_count[symbol]++;
				break;
			case DeclKind::Transparent:
				id_path = ElementPath(transparent_path, std::format("[{}]", transparent_count));
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
}
