#pragma once

#include "../not_statements/import_chain.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Bracketed import list with import chains as elements.
	 */
	class NestedImportList final:
		  public List<ImportChain, internal::NameGetters::nestedImportList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(NestedImportList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit NestedImportList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::NestedImportList;
		}

		static MBox<NestedImportList> parse(LangParserState& state);

		~NestedImportList() final = default;
	};
}
