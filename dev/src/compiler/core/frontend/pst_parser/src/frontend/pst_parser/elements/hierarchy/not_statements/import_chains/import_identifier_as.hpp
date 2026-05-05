#pragma once

#include "../import_chain.hpp"

namespace pst {
	/**
	 * @brief Import chain of the form `A.B.C` or `A.B.C as X`
	 */
	class ImportIdentifierAs final: public ImportChain {
		std::vector<AccessInternalAnonymous<IdentifierWrapper>>    names;
		NAMED_CHILD_OPT(as, IdentifierWrapper);

	public:
		explicit ImportIdentifierAs(const LangParserState& state): ImportChain(state) {
			this->element_kind = ElementKind::ImportIdentifierAs;
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
			return "Import Identifier As Chain";
		}

		static MBox<ImportIdentifierAs> parse(LangParserState& state);

		[[nodiscard]]
		bool isImportAs() const {
			return as.has_value();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> asWhat() const {
			return as.map([](const auto& acc) { return acc.give(); });
		}

		void dprint(std::ostream& out) const final;
		~ImportIdentifierAs() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
		void calcElementPathHashRecursive() override;
	};
}
