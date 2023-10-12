#include "pst.hpp"
#include <token_parser_core/automatic.hpp>

namespace pst {

	PST::PST(lexer::TokenData&& td):
		  token_data(std::forward<lexer::TokenData>(td)),

		  parser_state(
			  tpc::TokenStream(
				  token_data.tokens,
				  tpc::Token(token_data.eof_sentinel),
				  0,
				  token_data.tokens.size()
			  ),
			  ErrorState()
		  ) {
		top_level = TopLevel::parse(parser_state);

		err = std::move(parser_state.err);
	}

	const std::vector<tpc::ParserCBorrowRef<Import>>& PST::getImports() const {
		return parser_state.getImports();
	}

	const ErrorState& PST::getErrorState() const { return err; }

	void PST::dprint(std::ostream& out) const { nullAwareDprint(top_level, out); }

	ParserCBorrowRef<TopLevel> PST::getTopLevelElement() const { return top_level.borrow(); }
}
