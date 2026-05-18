#pragma once

#include "../meta.hpp"
#include "wrapper_elements/identifier_wrapper.hpp"

namespace pst {
	/**
	 * @brief Simple dotted name that is Identifiers separated by dots potentially ended by `.*`
	 *
	 * used for imports. Examples can be found in specific import chains.
	 */
	class ImportChain: public NotStmt {
	public:
		explicit ImportChain(const LangParserState& state): NotStmt(state) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import Chain";
		}

		[[nodiscard]]
		virtual usize numberOfNames() const
			= 0;

		[[nodiscard]]
		virtual AccessLocked<IdentifierWrapper> getNameByIndex(usize index) const
			= 0;

		static MBox<ImportChain> parse(LangParserState& state);
	};
}
