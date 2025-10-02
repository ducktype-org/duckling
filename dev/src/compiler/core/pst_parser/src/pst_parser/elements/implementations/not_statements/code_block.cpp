#include "../../hierarchy/not_statements/code_block.hpp"

#include "preamble.hpp"

#include "diagnostic/message.hpp"

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

		while (state.notEmpty()) {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);

			if (stmt) {
				out->statements.emplace_back(nullptr);
				state.parse(out).assign(&out->statements.back(), std::move(stmt));
			}

			while (state[0].is(Special::Semicolon)) {
				state.log(makeBox<error::DuplicateSemicolon>(state.getPosition()));
				state.tokens().skip();
			}
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
}
