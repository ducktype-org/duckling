#include "../../hierarchy/not_statements/code_block.hpp"

#include "preamble.hpp"

#include <algorithm>

namespace pst {
	void CodeBlock::cloneSubElements(const CodeBlock& other) {
		ELEMENT_CLONE_SUB_ELEMENT(statements);
		fillSymbols();
		ParentClass::cloneSubElements(other);
	}

	MBox<CodeBlock> CodeBlock::parse(LangParserState& state) {
		auto order_type = state.getContext()->block_order;

		CORE_ASSERT(
			order_type != BlockOrderType::Undefined, "Parsing with an undefined ordering type"
		);

		auto out = makeBox<CodeBlock>(state);

		out->type = order_type;

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.logInt(makeBox<error::BlockStartError>(state.getPosition()));
			return nullptr;
		}

		PARSE().goDown();

		// @TODO: #1484 Rethink parser errors
		PST_WHILE(state.notEmpty()) {
			MBox<Stmt> stmt;
			PARSE().one(&stmt);

			if (stmt) {
				out->statements.emplace_back(nullptr);
				PARSE().assign(&out->statements.back(), std::move(stmt));
			}

			PST_WHILE(state[0].is(Special::Semicolon)) {
				state.logInt(makeBox<error::DuplicateSemicolon>(state.getPosition()));
				state.tokens().skip();
			}
		}

		PARSE().goUpAndSkip();

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
				symbol = stmt.internal()->getInternalSymbolName().value();
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
		if (type == BlockOrderType::Ordered) {
			auto ordered = hashing::ComponentHash(path, "ordered");
			calcIndexedListChildPath<Stmt>({ statements }, ordered);
		} else if (type == BlockOrderType::Unordered) {
			auto unordered = hashing::ComponentHash(path, "unordered");
			calcOrderedListChildPath(statements, unordered);
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

	HashAlg& CodeBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
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
