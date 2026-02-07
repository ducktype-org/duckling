#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>

#include <ranges>

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition(query::Context& ctx) const {
		variant_match(value) {
			variant_case_novalue(GeneratedElement) { return std::nullopt; }
			variant_case(PstOrigin, pst_origin) {
				return pst_origin.getElement().unlock(ctx)->getSourcePosition();
			}
			variant_case(std::vector<PstOrigin>, pst_origins) {
				dia::SourcePosition first_position
					= pst_origins[0].getElement().unlock(ctx)->getSourcePosition();

				for (usize i{ 1 }; i < pst_origins.size(); ++i) {
					auto pos       = pst_origins[i].getElement().unlock(ctx)->getSourcePosition();
					first_position = dia::SourcePosition::merge(first_position, pos);
				}
				return first_position;
			}
		}
		CORE_UNREACHABLE();
	}

	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements) {
		return { pst_elements | std::views::transform(&PstOrigin::fromElement)
			     | std::ranges::to<std::vector<PstOrigin>>() };
	}

	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element) {
		return { PstOrigin::fromElement(pst_element) };
	}

	ElementOrigin generatedOrigin() { return { GeneratedElement{} }; }

	ElementOrigin ElementOrigin::appendToOrigin(
		ElementOrigin origin, pst::Access<pst::LangElement> pst_element
	) {
		variant_match(origin.value) {
			variant_case_novalue(GeneratedElement) {
				CORE_PANIC("Cannot append PST origin to generated origin.");
			}
			variant_case(PstOrigin, elem) {
				std::vector<PstOrigin> new_elems = { elem, PstOrigin::fromElement(pst_element) };
				return { new_elems };
			}
			variant_case(std::vector<PstOrigin>, elems) {
				std::vector<PstOrigin> new_elems = elems;
				new_elems.push_back(PstOrigin::fromElement(pst_element));
				return { new_elems };
			}
		}
		CORE_UNREACHABLE();
	}

	std::vector<pst::AccessLocked<pst::LangElement>> ElementOrigin::getPstElements() const {
		variant_match(value) {
			variant_case_novalue(GeneratedElement) {
				return std::vector<pst::AccessLocked<pst::LangElement>>{};
			}
			variant_case(PstOrigin, elem) { return std::vector{ elem.getElement() }; }
			variant_case(std::vector<PstOrigin>, elems) {
				return elems | std::views::transform(&PstOrigin::getElement)
				     | std::ranges::to<std::vector>();
			}
		}
		CORE_UNREACHABLE();
	}

	pst::AccessLocked<pst::LangElement> PstOrigin::getElement() const {
		return pst::LangElement::getByStableHash(hash);
	}

	PstOrigin PstOrigin::fromElement(pst::Access<pst::LangElement> element) {
		return PstOrigin{ element->getHash() };
	}
}
