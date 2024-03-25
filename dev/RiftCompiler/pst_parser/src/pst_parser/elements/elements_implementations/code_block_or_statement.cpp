#include "elements_implementation.hpp"
#include "pst_parser/elements/elements.hpp"
#include <base/variant.hpp>

namespace pst {
	ParserRef<CodeBlockOrStmt> CodeBlockOrStmt::parse(RiftParserState& state) {
		auto out = makeRef<CodeBlockOrStmt>(state.ctokens().peek().getPosition());
		if (state.ctokens().isBracketGroup(Token::BracketType::Curly))
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

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) {
				return const_iterator(&stmt);
			}
			variant_case(ParserRef<CodeBlock>, code_block) { 
				return code_block->begin(); 
			}
		}
		RIFT_PANIC("something went wrong");
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::end() const {
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) {
				return const_iterator(&stmt) + 1;
			}
			variant_case(ParserRef<CodeBlock>, code_block) { 
				return code_block->end(); 
			}
		}
		RIFT_PANIC("something went wrong");
	}
}
