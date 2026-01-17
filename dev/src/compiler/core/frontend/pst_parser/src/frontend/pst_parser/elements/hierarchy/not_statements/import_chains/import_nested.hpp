#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.*` or `A.B.* hides X, Y`
	 */
	class ImportNested final: public ImportChain {
		std::vector<tpc::Identifier> names;
		NAMED_CHILD(nested_import, NestedImportList);

	public:
		explicit ImportNested(const dia::SourcePosition& position): ImportChain(position) {
			this->element_kind = ElementKind::ImportNested;
		}

		[[nodiscard]]
		const std::vector<tpc::Identifier>& getNames() const {
			return names;
		}

		[[nodiscard]]
		auto begin() const {
			return names.cbegin();
		}

		[[nodiscard]]
		auto end() const {
			return names.cend();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import Star Hides Chain";
		}

		static MBox<DottedName> parse(LangParserState& state);

		[[nodiscard]]
		AccessLocked<NestedImportList> getNestedImportList() const {
			return nested_import.give();
		}

		void dprint(std::ostream& out) const final;
		~ImportNested() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
