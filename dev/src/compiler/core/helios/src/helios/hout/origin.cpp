#include "origin.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/source_position.hpp>
#include <diagnostic/stable_position.hpp>

namespace compiler::helios::code {
	base::Optional<dia::SourcePosition> ElementOrigin::getSourcePosition(query::Context& ctx) const {
		return source_position.map([&ctx](const dia::StablePosition& pos) {
			return pos.getActiveSourcePosition(ctx);
		});
	}

	base::Optional<dia::StablePosition> ElementOrigin::getStablePosition() const {
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
		return { pst_element->getStablePosition(), pst_element->getHash(), false };
	}

	ElementOrigin pstOriginOrdered(
		const ElementOrigin& origin, pst::Access<pst::LangElement> element_to_the_right
	) {
		auto source_position = origin.getStablePosition();
		if_opt_some(source_position, pos) {
			pos.extendWithSubsequentPos(element_to_the_right->getStablePosition());
		}
		if_opt_none(source_position) {
			source_position = element_to_the_right->getStablePosition();
		}
		return { source_position, {}, false };
	}

	ElementOrigin elementOriginOrdered(const ElementOrigin& left, const ElementOrigin& right) {
		auto lsp = left.getStablePosition();
		auto rsp = right.getStablePosition();
		if_opt_some(lsp, lpos) {
			if_opt_some(rsp, rpos) { lpos.extendWithSubsequentPos(rpos); }
			return { lpos, {}, false };
		}
		if_opt_none(lsp) { return { rsp, {}, rsp.empty() }; }
		CORE_UNREACHABLE();
	}

	ElementOrigin multiplePstOriginOrdered(
		const std::vector<pst::Access<pst::LangElement>>& ordered_pst_elements
	) {
		CORE_ASSERT(!ordered_pst_elements.empty(), "pst_elements cannot be empty");

		auto pos = ordered_pst_elements[0]->getStablePosition();

		for (usize i{ 1 }; i < ordered_pst_elements.size(); ++i)
			pos.extendWithSubsequentPos(ordered_pst_elements[i]->getStablePosition());

		return { pos, {}, false };
	}
}
