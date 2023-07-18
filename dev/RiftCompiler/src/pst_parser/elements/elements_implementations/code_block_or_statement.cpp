#include "elements_implementation.hpp"

namespace pst {
	ParserRef<CodeBlockOrStmt> CodeBlockOrStmt::parse(RiftParserState& state) {
		auto out = makeRef<CodeBlockOrStmt>();
		if (state.ctokens().is(Token::Type::CurlyGroup)) {
			out->content = CodeBlock::parse(state);
		} else {
			out->content = Stmt::parse(state);
		}

		return out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		struct PrinterFunctor {
			std::ostream& out_;

			void operator()(const ParserRef<Stmt>& stmt) {
				nullAwareDprint(stmt, out_);
			}
			void operator()(const ParserRef<CodeBlock>& codeBlock) {
				nullAwareDprint(codeBlock, out_);
			}
			PrinterFunctor(std::ostream& out) : out_(out) {}
		};
		out << "{\"CodeBlockOrStmt\": ";
		std::visit(PrinterFunctor(out), content);
		out << "}";
	}

	std::span<const ParserRef<Stmt>> CodeBlockOrStmt::getStatements() const {
		if (std::holds_alternative<ParserRef<Stmt>>(content)) {
			auto& stmt = std::get<ParserRef<Stmt>>(content);
			return std::span<const ParserRef<Stmt>, 1>{std::addressof(stmt), 1};
		}
		else {
			return std::get<ParserRef<CodeBlock>>(content)->getStatements();
		}
	}

}