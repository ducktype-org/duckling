#pragma once

#include "elements/elements_forward.hpp"

#include "elements/elements.hpp"  // toplevel only, @TODO: change it to something better

#include "rift_parser_base.hpp"
#include <token_parser_core/parser_state.hpp>
#include <diagnostic/error_state.hpp>

namespace pst {
	/**
	 * @brief Parse syntax tree
	 */
	class PST {
		lexer::TokenData    token_data;
		RiftParserState     parser_state;
		ParserRef<TopLevel> top_level;
		dia::ErrorState     err;


	public:
		PST(lexer::TokenData&& td);

		[[nodiscard]]
		const std::vector<tpc::ParserCBorrowRef<Import>>& getImports() const;
		[[nodiscard]]
		const dia::ErrorState& getErrorState() const;

		[[nodiscard]]
		ParserCBorrowRef<TopLevel> getTopLevelElement() const;

		PST(PST&& other):
			  token_data(std::move(other.token_data)),
			  parser_state(std::move(other.parser_state)),
			  top_level(std::move(other.top_level)),
			  err(std::move(other.err)) {}

		void dprint(std::ostream& out) const;
	};
}
