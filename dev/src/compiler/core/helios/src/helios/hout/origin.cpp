#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/stable_position.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/source_position.hpp>

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition() const {
		return source_position.map([](const pst::StablePosition& stable_pos) {
			return stable_pos.getActiveSourcePosition();
		});
	}

	ElementOrigin ElementOrigin::extended(pst::Access<pst::LangElement> pst_element) const {
		match_optional(source_position) {
			opt_some(pos) {
				pst::StablePosition new_pos = pos;
				new_pos.extendWith(pst_element->getStablePosition());
				return { new_pos, {}, false };
			}
			opt_none { return { pst_element->getStablePosition(), {}, false }; }
		}
		CORE_UNREACHABLE();
	}

	ElementOrigin ElementOrigin::generatedFrom() const { return { source_position, {}, true }; }

	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements) {
		if (pst_elements.empty()) return generatedOrigin();

		auto pos = pst_elements[0]->getStablePosition();

		for (usize i{ 1 }; i < pst_elements.size(); ++i)
			pos.extendWith(pst_elements[i]->getStablePosition());

		return { pos, {}, false };
	}

	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element) {
		return { pst_element->getStablePosition(), pst_element->getHash(), false };
	}

	ElementOrigin generatedOrigin() { return { {}, {}, true }; }

	base::Optional<pst::AccessLocked<pst::LangElement>> ElementOrigin::getPSTElement() const {
		return pst_element.map([](const pst::LangElement::HashType& hash) {
			auto element = pst::LangElement::getByStableHash(hash);
			return element;
		});
	}
}
