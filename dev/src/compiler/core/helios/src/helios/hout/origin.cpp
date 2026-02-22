#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/source_position.hpp>

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition() const {
		return source_position;
	}

	ElementOrigin ElementOrigin::extended(base::Optional<dia::SourcePosition> other_pos) const {
		base::Optional<dia::SourcePosition> new_pos;
		match_optional(source_position) {
			opt_some(pos) {
				match_optional(other_pos) {
					opt_some(other_pos_val) {
						new_pos = dia::SourcePosition::merge(pos, other_pos_val);
					}
				}
				opt_none { new_pos = pos; }
			}
			opt_none { new_pos = other_pos; }
		}
		return { new_pos, false };
	}

	ElementOrigin ElementOrigin::extended(const ElementOrigin& other) const {
		return extended(other.getSourcePosition());
	}

	ElementOrigin ElementOrigin::extended(const pst::Access<pst::LangElement> pst_element) const {
		return extended(pst_element->getSourcePosition());
	}

	ElementOrigin ElementOrigin::generatedFrom() const { return { source_position, true }; }

	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements) {
		auto pos = pst_elements[0]->getSourcePosition();

		for (usize i{ 1 }; i < pst_elements.size(); ++i)
			pos = dia::SourcePosition::merge(pos, pst_elements[i]->getSourcePosition());

		return { pos, false };
	}

	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element) {
		return { pst_element->getSourcePosition(), false };
	}

	ElementOrigin generatedOrigin() { return { {}, true }; }
}
