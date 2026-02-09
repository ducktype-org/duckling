#pragma once

#include <frontend/pst_parser/access.hpp>

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

#include <variant>

namespace compiler::helios::code {

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
		std::vector<PstOrigin> pst_origins;
		bool                   is_generated;

	public:
		ElementOrigin(std::vector<PstOrigin> pst_origins, bool is_generated):
			  pst_origins(std::move(pst_origins)),
			  is_generated(is_generated) {}

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
		 * @brief Helper function that creates a new origin based on the current one
		 * but with an additional PST element.
		 *
		 * If the current element is generated, then throws an error.
		 */
		ElementOrigin extended(pst::Access<pst::LangElement> pst_element);

		/**
		 * @brief Helper function that creates a new origin based on the current one but marked as
		 * generated. Used for example when the `deref expr` or `cast expr` is generated based on an
		 * existing expression.
		 */
		ElementOrigin generatedFrom();

		[[nodiscard]] bool isGenerated() const { return is_generated; }

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
