#include "pst_access_side_input.hpp"
#include "base/str/str_utils.hpp"

#include "../lang_parser_element.hpp"

namespace pst::internal {
	std::string PSTAccessKey::debugString() const {
		auto lang_elem = LangElement::getByStableHash(hash).illegalAccess().value();
		return std::format(
			R"({{ "file": "{}", "start": {}, "path": "{}", "content": "{}" }})",
			lang_elem->getSourcePosition().getSource()->getFile().getFilePath().strView(),
			lang_elem->getSourcePosition().getStart(),
			base::escapeString(lang_elem->getElementPathHash().str()),
			base::escapeString(lang_elem->getSourcePosition().content())
		);
	}
}
