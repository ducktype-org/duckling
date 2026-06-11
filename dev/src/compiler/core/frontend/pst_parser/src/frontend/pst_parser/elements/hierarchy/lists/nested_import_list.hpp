#pragma once

#include "../not_statements/import_chain.hpp"
#include "preamble.hpp"

namespace pst {
	namespace {
		using ImportParentList = List<ImportChain, internal::NameGetters::nestedImportList>;
	}

	/**
	 * @brief Bracketed import list with import chains as elements.
	 */
	class NestedImportList final: public ImportParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(NestedImportList, ImportParentList);
		CLONE_SUBELEMENTS();
	public:
		explicit NestedImportList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::NestedImportList;
		}

		static MBox<NestedImportList> parse(LangParserState& state);

		~NestedImportList() final = default;
	};
}
