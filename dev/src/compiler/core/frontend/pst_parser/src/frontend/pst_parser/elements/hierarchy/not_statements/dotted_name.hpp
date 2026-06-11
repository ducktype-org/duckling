#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Simple dotted name that is Identifiers separated by dots potentially ended by `.*`
	 *
	 * used for imports
	 */
	class DottedName final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(DottedName, NotStmt, star);
		CLONE_SUBELEMENTS();
	private:
		std::vector<AccessInternalAnonymous<IdentifierWrapper>> names;
		bool                                                    star = false;

	public:
		explicit DottedName(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::DottedName;
		}

		[[nodiscard]]
		usize numberOfNames() const {
			return names.size();
		}

		[[nodiscard]]
		const AccessLocked<IdentifierWrapper> getNameByIndex(usize index) const {
			return names[index].give();
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
			return "Dotted Name";
		}

		static MBox<DottedName> parse(LangParserState& state);

		[[nodiscard]]
		bool getStar() const;

		void dprint(std::ostream& out) const final;
		~DottedName() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
		void     calcElementPathHashRecursive() override;
	};
}
