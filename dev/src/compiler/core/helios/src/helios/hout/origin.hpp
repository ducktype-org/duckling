#pragma once

#include <frontend/pst_parser/access.hpp>

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

#include <variant>

namespace compiler::helios::code {
	/**
	 * @brief This struct represents the generated origin of the HOUT element.
	 */
	struct GeneratedElement {};

	/**
	 * @brief This struct represents the PST origin of the HOUT element.
	 *
	 * @note It stores the Bit256 hash of the original PST element instead of the reference to it,
	 * to enable working in the Language Server setting, where the PST can be re-parsed and nodes
	 * can reside in the new memory but the hash of elements can remain unchanged.
	 *
	 * For the single-forward-pipeline compiler mode this is invisible and does not affect anything.
	 */
	struct PstOrigin {
		base::Bit256 hash;

		[[nodiscard]] pst::AccessLocked<pst::LangElement> getElement() const;

		static PstOrigin fromElement(pst::Access<pst::LangElement> element);
	};

	class ElementOrigin {
		// In the future maybe template-generated?
		using ValueType = std::variant<GeneratedElement, PstOrigin, std::vector<PstOrigin>>;

		ValueType value;

	public:
		ElementOrigin(ValueType value): value(std::move(value)) {}

		/**
		 * @brief Get the source position of the origin, if it is based on a PST element. If the
		 * origin is generated, it returns std::nullopt.
		 *
		 * If the origin is based on multiple elements, they are merged into a single source
		 * position covering all of them. (or throws panic if they are from different sources).
		 */
		[[nodiscard]] base::Optional<dia::SourcePosition> getSourcePosition(query::Context& ctx
		) const;


		/**
		 * @brief Helper function that appends a PST element to an existing origin, creating a new
		 * origin with the updated PST element(s).
		 *
		 * If the current element is generated, then throws an error.
		 *
		 * If the current element is a single PST element, it creates a vector origin with
		 * the existing and the new element.
		 *
		 * If the current element is already a vector of PST
		 * elements, it appends the new value.
		 */
		static ElementOrigin appendToOrigin(
			ElementOrigin origin, pst::Access<pst::LangElement> pst_element
		);

		/**
		 * @brief Get the PST elements that this origin is based on.
		 * In case of a GeneratedElement origin, it returns an empty vector.
		 */
		[[nodiscard]] std::vector<pst::AccessLocked<pst::LangElement>> getPstElements() const;
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
