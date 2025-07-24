#pragma once

#include "lang_parser_element.hpp"
#include "pst_automatic.hpp"

#include <base/strongly_typed_id.hpp>

#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_state.hpp>

#include <utility>

namespace pst {
	/**
	 * @brief State used for parsing Duckling to PST
	 */
	class LangParserState final: public tpc::ParserState {
		std::vector<ImportType> imports;

	public:
		LangParserState(tpc::TokenStream&& tokens, Ref<dia::Logger> err):
			  tpc::ParserState(std::move(tokens), err) {}

		/**
		 * @brief Adds import to the list of imports.
		 */
		void addImport(const CRef<pst::Import>& import);

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
		template<std::derived_from<LangElement> El>
		pst::PSTAutomatic<LangParserState> parse(Box<El>& el) {
			return { *this, el.refMut() };
		}

		/**
		 * @brief Gives access to automatic parsing tools.
		 */
		template<std::derived_from<LangElement> El>
		pst::PSTAutomatic<LangParserState> parse(Ref<El> el) {
			return { *this, el };
		}
	};

}
