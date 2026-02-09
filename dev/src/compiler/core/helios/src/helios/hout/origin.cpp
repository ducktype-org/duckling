#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>

#include <ranges>

namespace compiler::helios::code {
	PstOrigin PstOrigin::fromElement(pst::Access<pst::LangElement> element) {
		return PstOrigin{ .hash = element->getHash() };
	}

	pst::AccessLocked<pst::LangElement> PstOrigin::getElement() const {
		return pst::LangElement::getByStableHash(hash);
	}

	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition(query::Context& ctx) const {
		dia::SourcePosition first_position
			= pst_origins[0].getElement().unlock(ctx)->getSourcePosition();

		for (usize i{ 1 }; i < pst_origins.size(); ++i) {
			auto pos       = pst_origins[i].getElement().unlock(ctx)->getSourcePosition();
			first_position = dia::SourcePosition::merge(first_position, pos);
		}
		return first_position;
	}

	ElementOrigin ElementOrigin::extended(pst::Access<pst::LangElement> pst_element) {
		std::vector<PstOrigin> new_elems = pst_origins;
		new_elems.push_back(PstOrigin::fromElement(pst_element));
		return { new_elems, false };
	}

	ElementOrigin ElementOrigin::generatedFrom() { return { pst_origins, true }; }

	std::vector<pst::AccessLocked<pst::LangElement>> ElementOrigin::getPstElements() const {
		return pst_origins | std::views::transform(&PstOrigin::getElement)
		     | std::ranges::to<std::vector>();
	}

	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements) {
		return { pst_elements | std::views::transform(&PstOrigin::fromElement)
			         | std::ranges::to<std::vector<PstOrigin>>(),
			     false };
	}

	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element) {
		return { std::vector<PstOrigin>{ PstOrigin::fromElement(pst_element) }, false };
	}

	ElementOrigin generatedOrigin() { return { {}, true }; }
}
