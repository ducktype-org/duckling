#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.*` or `A.B.* hides X, Y`
	 */
	class ImportStarHides final: public ImportChain {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ImportStarHides, ImportChain);
		CLONE_SUBELEMENTS();

	protected:
		std::vector<AccessInternalAnonymous<IdentifierWrapper>>                 names;
		base::Optional<std::vector<AccessInternalAnonymous<IdentifierWrapper>>> hides;

	public:
		explicit ImportStarHides(LangElementConstructionArgument state): ImportChain(state) {
			this->element_kind = ElementKind::ImportStarHides;
		}

		[[nodiscard]]
		usize numberOfNames() const final {
			return names.size();
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getNameByIndex(usize index) const final {
			return names[index].give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import Star Hides Chain";
		}

		static MBox<ImportStarHides> parse(LangParserState& state);

		[[nodiscard]]
		bool isImportHides() const {
			return hides.has_value();
		}

		[[nodiscard]]
		base::Optional<usize> numberOfHides() const {
			return hides.map([](const auto& list) { return list.size(); });
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getHideIndex(usize index) const {
			return hides.map([=](const auto& list) { return list[index].give(); });
		}

		void dprint(std::ostream& out) const final;
		~ImportStarHides() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
		void     calcElementPathHashRecursive() override;
	};
}
