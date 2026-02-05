#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>

#include "base/except/exceptions.hpp"
#include <base/extend_cpp/variant_match.hpp>

#include "diagnostic/source_position.hpp"

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition(query::Context& ctx) const {
		variant_match(value) {
			variant_case_novalue(GeneratedElement) { return std::nullopt; }
			variant_case(pst::AccessLocked<pst::LangElement>, elem) {
				return elem.unlock(ctx)->getSourcePosition();
			}
			variant_case(std::vector<pst::AccessLocked<pst::LangElement>>, elems) {
				dia::SourcePosition first_position
					= elems[0].unlock(ctx)->getSourcePosition();
				for (int i{ 1 }; i < elems.size(); ++i) {
					auto pos       = elems[i].unlock(ctx)->getSourcePosition();
					first_position = dia::SourcePosition::merge(first_position, pos);
				}
				return first_position;
			}
		}
        CORE_UNREACHABLE();
	}

	ElementOrigin multiplePstOrigin(
		const std::vector<pst::AccessLocked<pst::LangElement>>& pst_elements
	) {
		return { pst_elements };
	}

	ElementOrigin pstOrigin(pst::AccessLocked<pst::LangElement> pst_element) {
		return { pst_element };
	}

	ElementOrigin generatedOrigin() { return { GeneratedElement{} }; }

}
