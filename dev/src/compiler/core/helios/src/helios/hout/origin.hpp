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


		/**
		 * @brief Helper function that appends a PST element to an existing origin, creating a new
		 * origin with the updated PST element(s). If the current element is generated, then throws
		 * an error. If the current element is a single PST element, it creates a vector origin with
		 * the existing and the new element. If the current element is already a vector of PST
		 * elements, it appends the new value.
		 */
		static ElementOrigin appendToOrigin(
			ElementOrigin origin, pst::AccessLocked<pst::LangElement> pst_element
		);
	};

	ElementOrigin generatedOrigin();

	ElementOrigin pstOrigin(pst::AccessLocked<pst::LangElement> pst_element);

	ElementOrigin multiplePstOrigin(
		const std::vector<pst::AccessLocked<pst::LangElement>>& pst_elements
	);
}
