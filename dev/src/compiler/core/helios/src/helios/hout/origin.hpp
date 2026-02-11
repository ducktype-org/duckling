#pragma once

#include <frontend/pst_parser/access.hpp>

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

#include <variant>

namespace compiler::helios::code {

	class ElementOrigin {
		base::Optional<dia::SourcePosition> source_position;
		bool                                is_generated;

	public:
		ElementOrigin(base::Optional<dia::SourcePosition> source_position, bool is_generated):
			  source_position(source_position),
			  is_generated(is_generated) {}

		/**
		 * @brief Get the source position of the origin, if it is available.
		 */
		[[nodiscard]] base::Optional<dia::SourcePosition> getSourcePosition() const;


		/**
		 * @brief Helper function that creates a new origin based on the current one
		 * but extending the source position to include the additional PST element.
		 */
		ElementOrigin extended(pst::Access<pst::LangElement> pst_element);

		/**
		 * @brief Helper function that creates a new origin based on the current one but marked as
		 * generated. Used for example when the `deref expr` or `cast expr` is generated based on an
		 * existing expression.
		 */
		ElementOrigin generatedFrom();

		[[nodiscard]] bool isGenerated() const { return is_generated; }
	};

	/**
	 * @brief Creates an ElementOrigin for a HOUT-generated element.
	 */
	ElementOrigin generatedOrigin();

	/**
	 * @brief Creates an ElementOrigin from a single PST element.
	 */
	ElementOrigin pstOrigin(pst::Access<pst::LangElement> pst_element);

	/**
	 * @brief Creates an ElementOrigin from multiple PST elements.
	 */
	ElementOrigin multiplePstOrigin(const std::vector<pst::Access<pst::LangElement>>& pst_elements);
}
