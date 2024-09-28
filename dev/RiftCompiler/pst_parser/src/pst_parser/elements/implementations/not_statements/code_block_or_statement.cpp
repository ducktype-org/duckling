#include "preamble.hpp"

namespace pst {
	ParserRef<CodeBlockOrStmt> CodeBlockOrStmt::parse(RiftParserState& state) {
		auto out = makeRef<CodeBlockOrStmt>(state.getPosition());
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			ParserRef<CodeBlock> block;
			state.parse(out).one(&block);
			if (block == nullptr) return nullptr;
			out->content = std::move(block);
		} else {
			ParserRef<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (stmt == nullptr) return nullptr;
			out->content = std::move(stmt);
		}

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

	void CodeBlockOrStmt::semPrint(std::ostream& out) const {
		struct PrinterFunctor {
			std::ostream& out_;

			void operator()(const ParserRef<Stmt>& stmt) {
				nullAwareSemanticTokenPrint(stmt, out_);
			}

			void operator()(const ParserRef<CodeBlock>& codeBlock) {
				nullAwareSemanticTokenPrint(codeBlock, out_);
			}

			PrinterFunctor(std::ostream& out): out_(out) {}
		};

		out << "{\"CodeBlockOrStmt\": {";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "namespace",)";
		out << "\"content\": ";
		std::visit(PrinterFunctor(out), content);
		out << "}}";
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) { return const_iterator(&stmt); }
			variant_case(ParserRef<CodeBlock>, code_block) { return code_block->begin(); }
		}
		RIFT_PANIC("something went wrong");
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::end() const {
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) { return const_iterator(&stmt) + 1; }
			variant_case(ParserRef<CodeBlock>, code_block) { return code_block->end(); }
		}
		RIFT_PANIC("something went wrong");
	}
}
