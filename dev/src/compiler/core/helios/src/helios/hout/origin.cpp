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
				dia::SourcePosition first_position = elems[0].unlock(ctx)->getSourcePosition();
				for (usize i{ 1 }; i < elems.size(); ++i) {
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

	ElementOrigin ElementOrigin::appendToOrigin(
		ElementOrigin origin, pst::AccessLocked<pst::LangElement> pst_element
	) {
		variant_match(origin.value) {
			variant_case_novalue(GeneratedElement) {
				CORE_PANIC("Cannot append PST origin to generated origin.");
			}
			variant_case(pst::AccessLocked<pst::LangElement>, elem) {
				return multiplePstOrigin({ elem, pst_element });
			}
			variant_case(std::vector<pst::AccessLocked<pst::LangElement>>, elems) {
				std::vector<pst::AccessLocked<pst::LangElement>> new_elems = elems;
				new_elems.push_back(pst_element);
				return multiplePstOrigin(new_elems);
			}
		}
		CORE_UNREACHABLE();
	}

	std::vector<pst::AccessLocked<pst::LangElement>> ElementOrigin::getPstElements() const {
		variant_match(value) {
			variant_case_novalue(GeneratedElement) {
				return std::vector<pst::AccessLocked<pst::LangElement>>{};
			}
			variant_case(pst::AccessLocked<pst::LangElement>, elem) { return std::vector{ elem }; }
			variant_case(std::vector<pst::AccessLocked<pst::LangElement>>, elems) { return elems; }
		}
		CORE_UNREACHABLE();
	}
}
