#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.*` or `A.B.* hides X, Y`
	 */
	class ImportStarHides final: public ImportChain {
		std::vector<tpc::Identifier> names;
		base::Optional<std::vector<tpc::Identifier>> hides;

	public:
		explicit ImportStarHides(const dia::SourcePosition& position): ImportChain(position) {
			this->element_kind = ElementKind::ImportStarHides;
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
		bool isImportHides() const {
			return hides.has_value();
		}

		[[nodiscard]]
		base::Optional<std::vector<tpc::Identifier>> getHidesList() const {
			return hides;
		}

		void dprint(std::ostream& out) const final;
		~ImportStarHides() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
