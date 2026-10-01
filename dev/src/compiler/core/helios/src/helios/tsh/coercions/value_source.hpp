/**
 * @file value_source.hpp
 * @brief The main entry point given by HELIoS to TSH when performing a coercion. Contains the
 * information about the `ExpressionType` of the value and all of the `ExpressionType`s of the
 * values it's built out of (i.e. tuple/array literals).
 *
 * For example when coercing `(123, foo(), move some_box)` to properly create a fully built coercion
 * tree we need the `ExpressionType`s of all the tuple elements, not just the outer `Literal`
 * `ExpressionType`, this is exactly what `ValueSource` stores.
 */

#pragma once

#include "helios/tsh/kind.hpp"

#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include "base/except/exceptions.hpp"
#include <base/types/ints.hpp>

#include <utility>
#include <vector>

namespace compiler::tsh::coercions {

	/**
	 * @brief The value a coercion is asked about and the parts it is built out of.
	 */
	class ValueSource final {
	public:
		/**
		 * @brief Creates a single, non-nested value.
		 */
		[[nodiscard]]
		static ValueSource singleValueSource(const ExpressionType<>& value) {
			return { value, {} };
		}

		/**
		 * @brief A value the expression builds out of @p sub_parts, like a tuple literal, whose
		 * parts are expressions of their own with their own `ExpressionType`s.
		 */
		[[nodiscard]]
		static ValueSource nestedValueSource(
			const ExpressionType<>& value, std::vector<ValueSource> sub_parts
		) {
			// @note: other aggregate literals may be added in the future. Feel free to change the
			// assert then.
			CORE_ASSERT(
				value.getType().getKind() == Kind::Tuple,
				"Only tuple literals should go through nestedValueSource for now"
			);

			return { value, std::move(sub_parts) };
		}

		/**
		 * @brief The value itself.
		 */
		[[nodiscard]]
		const ExpressionType<>& getValue() const noexcept {
			return value;
		}

		/**
		 * @brief Whether the value is built out of parts.
		 */
		[[nodiscard]]
		bool isBuiltOutOfParts() const noexcept {
			return not parts.empty();
		}

		/**
		 * @brief The parts the value is built out of.
		 */
		[[nodiscard]]
		const std::vector<ValueSource>& getParts() const noexcept {
			return parts;
		}

		/**
		 * @brief The pointee of the value, after the `Deref` step has read it out which changes the
		 * value category of the value tree.
		 */
		[[nodiscard]] ValueSource dereferenced() const;

		/**
		 * @brief The part of the value at @p index.
		 */
		[[nodiscard]] ValueSource partAt(usize index) const;

	private:
		ValueSource(const ExpressionType<>& value, std::vector<ValueSource> parts):
			  value(value),
			  parts(std::move(parts)) {}

		/// The root value.
		ExpressionType<> value;
		/// The sub-parts of the `ValueSource` tree, if given.
		std::vector<ValueSource> parts;
	};
}
