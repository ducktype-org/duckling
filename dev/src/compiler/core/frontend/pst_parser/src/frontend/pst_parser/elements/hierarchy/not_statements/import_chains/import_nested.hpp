#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.(A, B.C.*)`
	 */
	class ImportNested final: public ImportChain {
		std::vector<AccessInternalAnonymous<IdentifierWrapper>>    names;
		NAMED_CHILD(nested_import, NestedImportList);

	public:
		explicit ImportNested(const LangParserState& state): ImportChain(state) {
			this->element_kind = ElementKind::ImportNested;
		}

		[[nodiscard]]
		usize numberOfNames() const final {
			return names.size();
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getNameIndex(usize index) const final {
			return names[index].give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import Nested";
		}

		static MBox<ImportNested> parse(LangParserState& state);

		[[nodiscard]]
		AccessLocked<NestedImportList> getNestedImportList() const {
			return nested_import.give();
		}

		void dprint(std::ostream& out) const final;
		~ImportNested() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
