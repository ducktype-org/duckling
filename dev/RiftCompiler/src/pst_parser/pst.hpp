#pragma once

#include <token_parser_core/error_state.hpp>
#include <token_parser_core/parser_state.hpp>

#include "elements/elements.hpp"  // toplevel only, @TODO: change it to something better
#include "elements/elements_forward.hpp"
#include "rift_parser_base.hpp"

namespace pst {
	/**
	 * @brief Parse syntax tree
	 */
	class PST {
		lexer::TokenData token_data;
		RiftParserState  parser_state;
		tpc::ErrorState  err;

		ParserRef<TopLevel> top_level;

	public:
		PST(lexer::TokenData&& td);

		const std::vector<tpc::ParserCBorrowRef<Import>>& getImports() const;
		const tpc::ErrorState&                            getErrorState() const;

		ParserCBorrowRef<TopLevel> getTopLevelElement() const;

		PST(PST&& other):
			  token_data(std::move(other.token_data)),
			  parser_state(std::move(other.parser_state)),
			  err(std::move(other.err)),
			  top_level(std::move(other.top_level)) {}

		void dprint(std::ostream& out) const;
	};
}  // namespace pst
