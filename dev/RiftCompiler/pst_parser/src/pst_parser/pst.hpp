#pragma once

#include "elements/elements.hpp"  // toplevel only, @TODO: change it to something better

#include "rift_parser_base.hpp"
#include <diagnostic/logger.hpp>
#include <token_file/file.hpp>

namespace pst {
	/**
	 * @brief Parse syntax tree
	 */
	class PST {
		tokenizer::OwnFile file;
		lexer::TokenData&        token_data;
		ParserRef<TopLevel>     top_level;
		dia::Logger             err;
		std::vector<ImportType> imports;


	public:
		PST(tokenizer::OwnFile&& td);

		[[nodiscard]]
		const std::vector<ParserCBorrowRef<Import>>& getImports() const;
		[[nodiscard]]
		const dia::Logger& getLogger() const;

		[[nodiscard]]
		ParserCBorrowRef<TopLevel> getTopLevelElement() const;

		PST(PST&& other):
				file(std::move(other.file)),
			  token_data(other.token_data),
			  top_level(std::move(other.top_level)),
			  err(std::move(other.err)),
			  imports(std::move(other.imports)) {}

		void dprint(std::ostream& out) const;
	};
}
