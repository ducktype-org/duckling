#pragma once

#include "../not_statements/import_chain.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Bracketed import list with import chains as elements.
	 */
	class NestedImportList final:
		  public List<ImportChain, internal::NameGetters::nestedImportList> {
	public:
		explicit NestedImportList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::NestedImportList;
		}

		static MBox<NestedImportList> parse(LangParserState& state);

		~NestedImportList() final = default;
	};
}
