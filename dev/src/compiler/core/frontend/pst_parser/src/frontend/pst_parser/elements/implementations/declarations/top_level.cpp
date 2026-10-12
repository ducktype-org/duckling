// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/declarations/top_level.hpp"

#include "preamble.hpp"

namespace pst {
	void TopLevel::cloneSubElements(const TopLevel& other) {
		ELEMENT_CLONE_SUB_ELEMENT(statements);
		fillSymbols();
		ParentClass::cloneSubElements(other);
	}

	MBox<TopLevel> TopLevel::parse(LangParserState& state) {
		auto order_type = state.getContext()->block_order;

		CORE_ASSERT(
			order_type != BlockOrderType::Undefined, "Parsing with an undefined ordering type"
		);

		auto out = makeBox<TopLevel>(state);

		out->type = order_type;

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
		out->fillSymbols();

		PST_RETURN out;
	}

	void TopLevel::fillSymbols() {
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

	void TopLevel::calcElementPathHashRecursive() {
		auto path = getElementPathHash();
		if (type == BlockOrderType::Ordered) {
			auto ordered = hashing::ComponentHash(path, "ordered");
			calcIndexedListChildPath<Stmt>({ statements }, ordered);
		} else if (type == BlockOrderType::Unordered) {
			auto unordered = hashing::ComponentHash(path, "unordered");
			calcOrderedListChildPath(statements, unordered);
		}
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

	HashAlg& TopLevel::addElementDataToStableHash(HashAlg& partial_hash) const {
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
