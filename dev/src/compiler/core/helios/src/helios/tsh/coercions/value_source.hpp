// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file value_source.hpp
 * @brief The main entry point given by HELIoS to TSH when performing a coercion. Contains the
 * information about the `ExpressionType` of the value and all of the `ExpressionType`s of the
 * values it's built out of (i.e. tuple/array literals).
 */

#pragma once

#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/kind.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <utility>
#include <vector>

namespace compiler::tsh::coercions {

	/**
	 * @brief Represents the value a coercion is asked about of it. This is used to provide the
	 * coercion tree with the information needed to perform ownership decisions on top of the
	 * `SymbolType` plan.
	 *
	 * In most coercion cases the `ValueSource`	is a single node with a single `ExpressionType` of
	 * the coerced value. For more complex cases like coercing tuple literals it creates a tree
	 * structure representing the `ExpressionType` of the outer tuple and the `ExpressionType`s of
	 * all sub-components.
	 *
	 * @example When coercing `(123, foo(), move some_box)` the `ValueSource` will contain:
	 * [ValueCategory == Temporary] (123, foo(), move some_box)
	 *		[ValueCategory == Literal] (123)
	 *		[ValueCategory == Temporary] (foo())
	 *		[ValueCategory == local, Forces Move] (move some_box)
	 * @example When coercing `var a: (i32, T, box T) = (123, foo(), move some_box)` the
	 *`ValueSource` will contain just one node: [ValueCategory == Local] (123, foo(), move some_box)
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
