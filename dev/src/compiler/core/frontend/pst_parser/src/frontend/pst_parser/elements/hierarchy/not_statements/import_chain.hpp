#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Simple dotted name that is Identifiers separated by dots potentially ended by `.*`
	 *
	 * used for imports. Examples can be found in specific import chains.
	 */
	class ImportChain: public NotStmt {
	public:
		explicit ImportChain(const dia::SourcePosition& position): NotStmt(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Import Chain";
		}

		static MBox<ImportChain> parse(LangParserState& state);
	};
}
