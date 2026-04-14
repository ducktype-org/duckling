#include "origin.hpp"

#include <diagnostic_interactive/hash_code_position.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/source_position.hpp>

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition() const {
		return source_position.map([](const dia_int::HashCodePosition& pos) {
			return pos.toSourcePositionIllegalAccess();
		});
	}

	base::Optional<dia_int::HashCodePosition> ElementOrigin::getHashCodePosition() const {
		return source_position;
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> ElementOrigin::getPSTElement() const {
		return pst_element.map([](const pst::HashType& hash) {
			auto element = pst::LangElement::getByStableHash(hash);
			return element;
		});
	}

	ElementOrigin ElementOrigin::generatedFrom() const { return { source_position, {}, true }; }

	ElementOrigin generatedOrigin() { return { {}, {}, true }; }

	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element) {
		return { pst_element->getHashCodePosition(), pst_element->getHash(), false };
	}

	ElementOrigin pstOrigin(const ElementOrigin& origin, pst::Access<pst::LangElement> pst_element) {
		auto source_position = origin.getHashCodePosition();
		if_opt_some(source_position, pos) { pos.extendWith(pst_element->getHashCodePosition()); }
		if_opt_none(source_position) { source_position = pst_element->getHashCodePosition(); }
		return { source_position, {}, false };
	}

	ElementOrigin elementOrigin(const ElementOrigin& left, const ElementOrigin& right) {
		auto lsp = left.getHashCodePosition();
		auto rsp = right.getHashCodePosition();
		if_opt_some(lsp, lpos) {
			if_opt_some(rsp, rpos) { lpos.extendWith(rpos); }
			return { lpos, {}, false };
		}
		if_opt_none(lsp) { return { rsp, {}, rsp.empty() }; }
		CORE_UNREACHABLE();
	}

	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements) {
		CORE_ASSERT(!pst_elements.empty(), "pst_elements cannot be empty");

		auto pos = pst_elements[0]->getHashCodePosition();

		for (usize i{ 1 }; i < pst_elements.size(); ++i)
			pos.extendWith(pst_elements[i]->getHashCodePosition());

		return { pos, {}, false };
	}

}
