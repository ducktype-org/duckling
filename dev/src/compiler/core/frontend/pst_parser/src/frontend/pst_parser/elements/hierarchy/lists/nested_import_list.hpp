#pragma once

#include "../not_statements/import_chain.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration parameter list.
	 */
	class NestedImportList final:
		  public List<ImportChain, internal::NameGetters::nestedImportList> {
	public:
		explicit NestedImportList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::NestedImportList;
		}

		static MBox<NestedImportList> parse(LangParserState& state);

		~NestedImportList() final = default;
	};
}
