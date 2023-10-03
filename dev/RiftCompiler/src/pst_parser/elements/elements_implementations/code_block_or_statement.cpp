#include <base/variant.hpp>

#include "elements_implementation.hpp"

namespace pst {
	ParserRef<CodeBlockOrStmt> CodeBlockOrStmt::parse(RiftParserState& state) {
		auto out = makeRef<CodeBlockOrStmt>(state.ctokens().peek().getPosition());
		if (state.ctokens().is(Token::Type::CurlyGroup))
			out->content = CodeBlock::parse(state);
		else
			out->content = Stmt::parse(state);

		return out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		struct PrinterFunctor {
			std::ostream& out_;

			void operator()(const ParserRef<Stmt>& stmt) { nullAwareDprint(stmt, out_); }

			void operator()(const ParserRef<CodeBlock>& codeBlock) {
				nullAwareDprint(codeBlock, out_);
			}

			PrinterFunctor(std::ostream& out): out_(out) {}
		};

		out << "{\"CodeBlockOrStmt\": ";
		std::visit(PrinterFunctor(out), content);
		out << "}";
	}

	std::span<const ParserRef<Stmt>> CodeBlockOrStmt::getStatements() const {
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) {
				return std::span<const ParserRef<Stmt>, 1>{ std::addressof(stmt), 1 };
			}
			variant_case(ParserRef<CodeBlock>, code_block) { return code_block->getStatements(); }
		}
		RIFT_PANIC("something went wrong");
	}

}  // namespace pst
