#pragma once

#include <frontend/pst_parser/access.hpp>

#include "base/collections/optional.hpp"

#include "diagnostic/source_position.hpp"

#include <variant>

namespace compiler::helios::code {
	struct GeneratedElement {};

	class ElementOrigin {
		using ValueType = std::variant<
			GeneratedElement,
			pst::AccessLocked<pst::LangElement>,
			std::vector<pst::AccessLocked<pst::LangElement>>>;

		ValueType value;
	public:
		ElementOrigin(ValueType value): value(std::move(value)) {}

		[[nodiscard]] base::Optional<dia::SourcePosition> getSourcePosition(query::Context& ctx
		) const;
	};

	ElementOrigin generatedOrigin();

	ElementOrigin pstOrigin(pst::AccessLocked<pst::LangElement> pst_element);

	ElementOrigin multiplePstOrigin(
		const std::vector<pst::AccessLocked<pst::LangElement>>& pst_elements
	);
}
