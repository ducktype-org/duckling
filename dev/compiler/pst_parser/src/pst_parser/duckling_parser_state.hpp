#pragma once

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/automatic.hpp>
#include <base/strongly_typed_id.hpp>
#include <utility>

#include "duckling_parser_element.hpp"
#include "pst_automatic.hpp"

namespace pst {
	/**
	 * @brief State used for parsing Duckling to PST
	 */
	class DucklingParserState final: public tpc::ParserState {
		std::vector<ImportType> imports;

	public:
		DucklingParserState(tpc::TokenStream&& tokens, dia::Logger& err):
			  tpc::ParserState(std::move(tokens), err) {}

		/**
		 * @brief Adds import to the list of imports.
		 */
		void addImport(const tpc::ParserCBorrowRef<pst::Import>& import);

		/**
		 * @brief Extracts imports from state.
		 *
		 * @note Leaves State in an `illegal` state.
		 */
		[[nodiscard]]
		auto extractState() && -> std::vector<ImportType> {
			return std::move(imports);
		}

		/**
		 * @brief Gives access to automatic parsing tools.
		 */
		template<std::derived_from<DucklingElement> El>
		pst::PSTAutomatic<DucklingParserState> parse(ParserRef<El>& el) {
			return { *this, el.borrow_mut() };
		}

		/**
		 * @brief Gives access to automatic parsing tools.
		 */
		template<std::derived_from<DucklingElement> El>
		pst::PSTAutomatic<DucklingParserState> parse(ParserBorrowRef<El>& el) {
			return { *this, el };
		}
	};

}
