#pragma once

#include "format_sub_element.hpp"

namespace pst {
	/**
	 * @brief Element representing a string value in an expression
	 */
	class FormatSubString final: public FormatSubElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FormatSubString, FormatSubElement, string);

	protected:
		tpc::StringValue string;

	public:
		[[nodiscard]]
		tpc::StringValue getValue() const {
			return string;
		}

		explicit FormatSubString(LangElementConstructionArgument state): FormatSubElement(state) {
			this->element_kind = ElementKind::FormatSubString;
		}

		static MBox<FormatSubString> parse(LangParserState& state);
		~FormatSubString() override = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Format Sub String";
		}
	};
}
