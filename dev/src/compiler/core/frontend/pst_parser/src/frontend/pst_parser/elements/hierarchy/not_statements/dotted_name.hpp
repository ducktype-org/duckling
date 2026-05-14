#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Simple dotted name that is Identifiers separated by dots potentially ended by `.*`
	 *
	 * used for imports
	 */
	class DottedName final: public NotStmt {
		std::vector<tpc::Identifier> names;
		bool                         star = false;

	public:
		explicit DottedName(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::DottedName;
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
			return "Dotted Name";
		}

		static MBox<DottedName> parse(LangParserState& state);

		[[nodiscard]]
		bool getStar() const;

		void dprint(std::ostream& out) const final;
		~DottedName() final = default;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}
