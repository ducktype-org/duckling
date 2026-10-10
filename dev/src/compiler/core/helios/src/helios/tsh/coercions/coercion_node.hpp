// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file coercion_node.hpp
 * @brief One node of a coercion tree.
 *
 * A node tells about exactly one value (one part of the expression being coerced). Everything done
 * to that value is a step of this node, in the order it happens in, and every step says the type
 * of what it produces, so that the steps account for every change of the type from the source to
 * the target. Additionally, for values created from other values an `Elementwise` step stores the
 * sub-coercions. Currently this is only done in case of tuples.
 *
 * For example when coercing `(ref_i32, 123i32)` to `(i64 | f64, i32)`, the root will contain an
 * `Elementwise` step with two children. First of them being the node representing the `ref_i32 ->
 * i64 | f64` which will contain three steps: [Deref, Numeric, VariantPack]. The second one will
 * have the step list empty, since nothing is done to the next value.
 *
 * Every step kind states how good of a match it is (`RANK`) and what it is called (`NAME`), and
 * both are read through `std::visit`, so that a step kind added later without them does not
 * build. A step that has a rank of its own has it under its own name, like `Numeric` and
 * `Rank::Numeric`.
 */

#pragma once

#include "../symbol_type.hpp"
#include "coercion_rank.hpp"
#include "reference_coercion.hpp"

#include <helios/symbols/symbol_id.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/ints.hpp>

#include <algorithm>
#include <concepts>
#include <iosfwd>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace compiler::tsh::coercions {
	struct CoercionNode;

	namespace coercion_step {
		/**
		 * @brief The value is handed over to the location it is coerced into, which is where it may
		 * be copied, moved or refused.
		 *
		 * @note Deciding WHERE the value has to be passed is decided on the `SymbolType` level.
		 * This node is added every time when a value is passed into a new location. The fact HOW
		 * the value should be passed is decided on the `ExpressionType` level based on the
		 * ValueCategories.
		 *
		 * Thus, this node is inserted only in the `SymbolTypeCoercion` tree and `coercionOf` remaps
		 * it when ValueCategories are taken into account. The `HandOver` node can be remapped to:
		 * - Nothing:
		 * 		When copying by bytes is enough (e.g. user wrote a `move`, for a
		 * 		literal, an lvalue of a trivially copyable type)
		 * - `ImplicitMove`:
		 * 		When the value is a temporary
		 * - `ElementWise`:
		 * 		When the value is built out of parts (i.e. a tuple literal). Each part
		 * 		is then handed over separately.
		 * - Error:
		 * 		When a user was supposed to write `copy` or `move` explicitly.
		 */
		struct HandOver final {
			static constexpr Rank             RANK = Rank::Identity;
			static constexpr std::string_view NAME = "hand over";
		};

		/**
		 * @brief The value is implicitly moved since it's owned by the expression that produced it.
		 * @note Only a `Coercion` contains this step, after `HandOver` was replaced with it.
		 * @see HandOver docs
		 */
		struct ImplicitMove final {
			static constexpr Rank             RANK = Rank::Identity;
			static constexpr std::string_view NAME = "implicit move";
		};

		/**
		 * @brief Only the mutability the value is seen with changes. The value itself doesn't
		 * change.
		 */
		struct MutabilityChange final {
			static constexpr Rank             RANK = Rank::MutabilityChange;
			static constexpr std::string_view NAME = "mutability change";
		};

		/**
		 * @brief The pointee is read out of a `ref`/`box`.
		 */
		struct Deref final {
			static constexpr Rank             RANK = Rank::Deref;
			static constexpr std::string_view NAME = "deref";
		};

		/**
		 * @brief The value is widened into a wider numeric type.
		 */
		struct Numeric final {
			static constexpr Rank             RANK = Rank::Numeric;
			static constexpr std::string_view NAME = "numeric";
		};

		/**
		 * @brief The value is packed into the variant as the alternative at the given index.
		 */
		struct VariantPack final {
			static constexpr Rank             RANK = Rank::VariantPack;
			static constexpr std::string_view NAME = "pack as alternative";

			usize alternative{ 0 };
		};

		/**
		 * @brief The value is lifted into the `type` type.
		 */
		struct LiftToType final {
			static constexpr Rank             RANK = Rank::LiftToType;
			static constexpr std::string_view NAME = "lift to type";
		};

		/**
		 * @brief An implicit user conversion that builds the new value out of the old one.
		 * @TODO: #3656 Make use of this.
		 */
		struct UserConversion final {
			static constexpr Rank             RANK = Rank::UserConversion;
			static constexpr std::string_view NAME = "user conversion";

			helios::SymID function;
		};

		/**
		 * @brief The value is compared against the zero of its own type, which turns it into a
		 * `bool`.
		 */
		struct ZeroCheck final {
			static constexpr Rank             RANK = Rank::ZeroCheck;
			static constexpr std::string_view NAME = "zero check";
		};

		/**
		 * @brief A `void` expression is seen as one of another type.
		 */
		struct RetypeVoid final {
			static constexpr Rank             RANK = Rank::RetypeVoid;
			static constexpr std::string_view NAME = "retype void";
		};

		/**
		 * @brief A composite type is coerced element by element. Each element is a coercion node
		 * of its own. This step is as good as the worst thing done by `parts`.
		 */
		struct Elementwise final {
			static constexpr std::string_view NAME = "elementwise";

			std::vector<CoercionNode> parts;
		};
	}

	/**
	 * @brief One thing done to the value of a node, and the type it produces.
	 */
	struct CoercionStep final {
		using Kind = std::variant<
			coercion_step::HandOver,
			coercion_step::ImplicitMove,
			coercion_step::MutabilityChange,
			coercion_step::Deref,
			coercion_step::Numeric,
			coercion_step::VariantPack,
			coercion_step::LiftToType,
			coercion_step::UserConversion,
			coercion_step::ZeroCheck,
			coercion_step::RetypeVoid,
			coercion_step::Elementwise>;

		/// What the step does.
		Kind kind;
		/// The type of the value once the step is done.
		SymbolType<> result;

		/**
		 * @brief How good of a match this step is, its parts included.
		 */
		[[nodiscard]] CoercionRank rank() const;

		/**
		 * @brief What the step is called.
		 */
		[[nodiscard]]
		std::string_view name() const {
			return VISIT(kind, name, { return std::decay_t<decltype(name)>::NAME; });
		}

		/**
		 * @brief The step. The first line is written as is. Every line after it is prefixed
		 * with @p indent.
		 */
		void debugPrint(std::ostream& out, std::string_view indent = "") const;
	};

	/**
	 * @brief Everything that happens to a value on its way into a location of the target type.
	 */
	struct CoercionNode final {
		/// The source type of this nodes value.
		SymbolType<> source;

		/// The target type of this node.
		SymbolType<> target;

		/// Everything that has to be done to a value, in order it has to be done.
		std::vector<CoercionStep> steps{};

		/// How good of a match this coercion node is, with the sub-parts included.
		CoercionRank rank = CoercionRank::identity();

		/**
		 * @brief Adds a new @p step to the current node.
		 */
		void append(CoercionStep step) {
			rank = CoercionRank::combine(rank, step.rank());
			steps.push_back(std::move(step));
		}

		/**
		 * @brief The current type of the value.
		 */
		[[nodiscard]]
		const SymbolType<>& current() const noexcept {
			return steps.empty() ? source : steps.back().result;
		}

		/**
		 * @brief Whether nothing at all happens to the value at runtime, so it can be used as
		 * it is, i.e. a coercion that only relaxes mutability.
		 *
		 * @note `HandOver` and `ImplicitMove` are not no-ops, because the value still has to be
		 * copied or moved into its new place.
		 */
		[[nodiscard]]
		bool isNoOp() const noexcept {
			return steps.empty()
			    || (steps.size() == 1
			        && v_matches(steps.front().kind, coercion_step::MutabilityChange));
		}

		/**
		 * @brief Whether this node derefs. Deref is always the first step.
		 */
		[[nodiscard]]
		bool derefs() const noexcept {
			return not steps.empty() && v_matches(steps.front().kind, coercion_step::Deref);
		}

		/**
		 * @brief What happens to the reference part of the value of this node.
		 */
		[[nodiscard]]
		ReferenceCoercion referenceCoercion() const {
			return referenceCoercionRule(
				source.getRefKind(), derefs() ? ReferenceKind::Direct : source.getRefKind()
			);
		}

		/**
		 * @brief Whether this value is implicitly moved.
		 */
		[[nodiscard]]
		bool implicitlyMoves() const noexcept {
			return std::ranges::any_of(steps, [](const CoercionStep& step) {
				return v_matches(step.kind, coercion_step::ImplicitMove);
			});
		}

		/**
		 * @brief The node and its parts, one step per line.
		 *
		 * Every line is prefixed with @p indent, the first one included.
		 */
		void debugPrint(std::ostream& out, std::string_view indent = "") const;
	};

	inline CoercionRank CoercionStep::rank() const {
		return VISIT(kind, step, {
			using Step = std::decay_t<decltype(step)>;

			if constexpr (std::same_as<Step, coercion_step::Elementwise>) {
				CoercionRank worst = CoercionRank::identity();
				for (const CoercionNode& part: step.parts)
					worst = CoercionRank::combine(worst, part.rank);
				return worst;
			} else {
				return CoercionRank::of(Step::RANK);
			}
		});
	}
}
