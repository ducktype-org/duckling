#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.C` or `A.B.C as X`
	 */
	class ImportIdentifierAs final: public ImportChain {
		std::vector<tpc::Identifier>    names;
		base::Optional<tpc::Identifier> as;

	public:
		explicit ImportIdentifierAs(const LangParserState& state): ImportChain(state) {
			this->element_kind = ElementKind::ImportIdentifierAs;
		}

		[[nodiscard]]
		const std::vector<tpc::Identifier>& getNames() const final {
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
			return "Import Identifier As Chain";
		}

		static MBox<ImportIdentifierAs> parse(LangParserState& state);

		[[nodiscard]]
		bool isImportAs() const {
			return as.has_value();
		}

		[[nodiscard]]
		base::Optional<tpc::Identifier> asWhat() const {
			return as;
		}

		void dprint(std::ostream& out) const final;
		~ImportIdentifierAs() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
