#include "../../hierarchy/not_statements/code_block.hpp"

#include "preamble.hpp"

#include <algorithm>

namespace pst {
	MBox<CodeBlock> CodeBlock::parse(LangParserState& state, CodeBlockType order_type) {
		CORE_ASSERT(order_type != Undefined, "Parsing with an undefined ordering type");

		auto position = state.getPosition();
		auto out      = makeBox<CodeBlock>(position);

		out->type = order_type;

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			out->statements.emplace_back(nullptr);
			state.parse(out).assign(&out->statements.back(), std::move(stmt));
		}

		state.parse(out).goUpAndSkip();

		out->fillSymbols();

		return out;
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

	void CodeBlock::calcElementPathsRecursive() {
		auto path = getElementPath();
		if (type == Ordered) {
			auto ordered = ElementPath(path, "ordered");
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

	LangElement::HashAlg& CodeBlock::calcStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, statements.size());
		addToHash(partial_hash, type);
		if (type == CodeBlockType::Unordered) {
			std::vector<std::pair<std::string, usize>> symbols_available_data;
			for (auto& [name, vec]: by_symbol)
				symbols_available_data.emplace_back(name.str(), vec.size());
			std::ranges::sort(symbols_available_data);
			addToHash(partial_hash, symbols_available_data);
		}
		return partial_hash;
	}
}
