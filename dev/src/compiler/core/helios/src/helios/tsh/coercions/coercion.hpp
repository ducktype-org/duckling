/**
 * @file coercion.hpp
 *
 * @brief Everything the type system decides about handing a value over to a location
 * of a possibly different type. `Coercion` represents a tree of what has to happen to the value or
 * the reason nothing can happen.
 *
 * Both coercion layers(`SymbolType` and `ExpressionType`) answer with the same tree:
 * - `SymbolTypeCoercion` is decided from the types alone, so it knows nothing about ownership.
 *   This is what `QuerySymbolTypeCoercion` caches.
 * - `Coercion` is the same tree decided for one particular value, which is the `ExpressionType`.
 *   Every `HandOver` is decided: it is gone, or it is an `ImplicitMove`. This is used by the rest
 *   of the compiler.
 *
 * A successful coercion is represented by a `CoercionNode` (@see `coercion_node.hpp`).
 * A refusal is represented by a `CoercionError` (@see coercion_error.hpp).
 */

#pragma once

#include "../expression_type.hpp"
#include "../symbol_type.hpp"
#include "coercion_error.hpp"
#include "coercion_node.hpp"
#include "coercion_rank.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <concepts>
#include <iosfwd>
#include <string_view>
#include <utility>
#include <variant>

namespace compiler::tsh::coercions {
	template<typename Source>
	concept CoercionSource
		= std::same_as<Source, SymbolType<>> || std::same_as<Source, ExpressionType<>>;

	/**
	 * @brief A whole coercion of a value described by @p Source, or the reason it is refused.
	 */
	template<CoercionSource Source>
	class CoercionTree final {
	public:
		/**
		 * @brief A coercion of @p source that succeeded, and how it's done @p root.
		 */
		[[nodiscard]]
		static CoercionTree successfulCoercion(const Source& source, CoercionNode root) {
			return { source, std::move(root) };
		}

		/**
		 * @brief A coercion of @p source that cannot be performed, and why @p error.
		 */
		[[nodiscard]]
		static CoercionTree refusedCoercion(const Source& source, CoercionError error) {
			return { source, std::move(error) };
		}

		/**
		 * @brief What the coercion was asked about.
		 */
		[[nodiscard]]
		const Source& getSource() const noexcept {
			return source;
		}

		/**
		 * @brief The location the value is handed to.
		 */
		[[nodiscard]]
		const SymbolType<>& getTarget() const {
			return isValid() ? getRoot().target : getError().target;
		}

		[[nodiscard]]
		bool isValid() const noexcept {
			return v_matches(outcome, CoercionNode);
		}

		[[nodiscard]]
		bool isRefused() const noexcept {
			return not isValid();
		}

		/**
		 * @brief Why the coercion may not be performed.
		 */
		[[nodiscard]]
		const CoercionError& getError() const {
			CORE_ASSERT(isRefused(), "Retrieving CoercionError from a successful coercion.");
			return v_get(outcome, CoercionError);
		}

		/**
		 * @brief Everything that happens to the value in the coercion, which is the root of the tree.
		 */
		[[nodiscard]]
		const CoercionNode& getRoot() const {
			CORE_ASSERT(isValid(), "Retrieving CoercionNode from a failed coercion.");
			return v_get(outcome, CoercionNode);
		}

		/**
		 * @brief How good of a match this coercion is.
		 */
		[[nodiscard]]
		CoercionRank getRank() const {
			return getRoot().rank;
		}

		/**
		 * @brief Whether nothing at all happens to the value, so that it may be used as it is (i.e.
		 * a coercion that only relaxes mutability)
		 */
		[[nodiscard]]
		bool isNoOp() const {
			return isValid() && getRoot().isNoOp();
		}

		/**
		 * @brief Whether the value itself is handed over to its new owner rather than copied.
		 */
		[[nodiscard]]
		bool implicitlyMoves() const requires std::same_as<Source, ExpressionType<>> {
			return isValid() && getRoot().implicitlyMoves();
		}

		/**
		 * @brief The whole coercion tree, one step per line.
		 *
		 * The first line is written as is. Every line after it is prefixed with @p indent.
		 */
		void debugPrint(std::ostream& out, std::string_view indent = "") const;

	private:
		/**
		 * @brief A coercion either is everything it is made of, or the reason it is nothing.
		 */
		using Storage = std::variant<CoercionNode, CoercionError>;

		CoercionTree(const Source& source, Storage outcome):
			  source(source),
			  outcome(std::move(outcome)) {}

		/// What the coercion was asked about.
		Source source;

		/// Either the tree or why there is none.
		Storage outcome;
	};

	/**
	 * @brief A coercion decided from the types alone. Doesn't contain any ownership decisions.
	 */
	using SymbolTypeCoercion = CoercionTree<SymbolType<>>;

	/**
	 * @brief A coercion decided for one particular value, with ownership included.
	 */
	using Coercion = CoercionTree<ExpressionType<>>;

}
