#include "../../hierarchy/not_statements/code_block.hpp"

#include "preamble.hpp"

#include <algorithm>

namespace pst {


	MBox<CodeBlock> CodeBlock::parse(LangParserState& state, BlockOrderType order_type) {
		CORE_ASSERT(order_type != Undefined, "Parsing with an undefined ordering type");

		auto position = state.getPosition();
		auto out      = makeBox<CodeBlock>(position);

		out->type = order_type;

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.logInt(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		// @TODO: #1484 Rethink parser errors
		PST_WHILE(state.notEmpty()) {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);

			if (stmt) {
				out->statements.emplace_back(nullptr);
				state.parse(out).assign(&out->statements.back(), std::move(stmt));
			}

			PST_WHILE(state[0].is(Special::Semicolon)) {
				state.logInt(makeBox<error::DuplicateSemicolon>(state.getPosition()));
				state.tokens().skip();
			}
		}

		state.parse(out).goUpAndSkip();

		out->fillSymbols();

		PST_RETURN out;
	}

	void CodeBlock::fillSymbols() {
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

	void CodeBlock::calcElementPathHashRecursive() {
		auto path = getElementPathHash();
		if (type == Ordered) {
			auto ordered = hashing::ComponentHash(path, "ordered");
			calcIndexedListChildPath<Stmt>({ statements }, ordered);
		} else if (type == Unordered) {
			calcOrderedListChildPath(statements, path);
		}
	}

	void CodeBlock::dprint(std::ostream& out) const {
		out << "[";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]";
	}

	LangElement::HashAlg& CodeBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, statements.size());
		addToHash(partial_hash, type);
		if (type == BlockOrderType::Unordered) {
			addToHash(partial_hash, no_symbol.size());
			addToHash(partial_hash, transparent.size());
			std::vector<std::pair<std::string, usize>> symbols_available_data;
			for (auto& [name, vec]: by_symbol)
				symbols_available_data.emplace_back(name.strView(), vec.size());
			std::ranges::sort(symbols_available_data);
			addToHash(partial_hash, symbols_available_data);
		} else { /*intentionally empty*/
		}
		return partial_hash;
	}
}
