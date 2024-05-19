#include "pst.hpp"
#include <token_parser_core/automatic.hpp>

namespace pst {

	using lsp::LSPTopLevel;

	PST::PST(tokenizer::OwnFile&& file):
		  file(std::move(file)),
		  token_data(this->file->getTokenData()),
		  err(this->file->getLogger()) {
		RiftParserState state(
			tpc::TokenStream(
				token_data.tokens, tpc::Token(token_data.eof_sentinel), 0, token_data.tokens.size()
			),
			err
		);
		top_level = TopLevel::parse(state);
		imports   = std::move(state).extractState();
	}

	const std::vector<tpc::ParserCBorrowRef<Import>>& PST::getImports() const { return imports; }

	const dia::Logger& PST::getLogger() const { return err; }

	void PST::dprint(std::ostream& out) const { nullAwareDprint(top_level, out); }

	void PST::getLSP(std::ostream& out) const {
		ParserRef<LSPTopLevel> lspTopLevel = top_level->topLevelFromPST();
		lspTopLevel->lsp_print(out);
	}

	ParserCBorrowRef<TopLevel> PST::getTopLevelElement() const { return top_level.borrow(); }
}
