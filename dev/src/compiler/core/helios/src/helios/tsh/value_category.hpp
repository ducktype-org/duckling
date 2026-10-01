// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file value_category.hpp
 * @brief Value category definition.
 *
 * Value category describes properties of the value that are not directly tied to its type.
 * Every ExpressionType has a ValueCategory object associated with it.
 */

#pragma once

#include <helios/symbols/symbol_id.hpp>

#include <base/extend_cpp/flag.hpp>
#include <base/types/bit256.hpp>

#include <hashing/hash.hpp>

#include <string>
#include <string_view>

namespace compiler::tsh {
	// There used to be "Identifiable" category, but it is now replaced with "Local" and "Global"
	// Maybe in the future we want to bring back "Identifiable" and make a struct to keep more
	// information
	/**
	 * Primary category describes source of the value.
	 */
	enum class PrimaryCategory {
		Temporary,    /**< Product of expression evaluation. An owned rvalue. */
		Local,        /**< A local variable. An owned lvalue. */
		Global,       /**< A global variable. A non-owned lvalue. */
		Literal,      /**< A value written explicitly in the code. A non-owned rvalue. */
		Dereferenced, /**< A location reached by dereferencing a pointer/reference/box. A non-owned
		                   lvalue. It may be read or assigned to, but not moved out of, because this
		                   expression does not own the pointee. */
	};

	/**
	 * @brief Get the primary category of a symbol: either Local or Global.
	 * @param ctx The query context, needed to distinguish local variables from global ones.
	 * @param symbol The symbol in question.
	 * @return The symbol's primary category.
	 */
	PrimaryCategory primaryCategoryOfSymbol(query::Context& ctx, compiler::helios::SymID symbol);

	/**
	 * @brief The name of a primary category.
	 */
	[[nodiscard]] std::string_view toString(PrimaryCategory category);
}

/**
 * @brief Flags describing what may be done with a value.
 * Each `ValueCategory` carries an `allows_semantic` set (what may be done) and a `force_semantic`
 * set (what must be done).
 *
 * - **MOVE**    The value is owned by the current scope, so ownership may be moved out of it. True
 * 				 for `Temporary` and `Local`. Set in `force_semantic` by the `move` operator.
 * - **COPY**    The value may be copied. Set for every category, because whether a copy
 *               is possible is a property of the AbstractType::isCopyable, not of the value
 * category.
 * - **REINIT**  The value is an addressable storage location that may be re-assigned - lvalue. True
 * 				 for `Local` and `Global`.
 * - **USE**     The value may be read. Currently set for every category.
 * - **DESTROY** The value's destructor runs at the end of the current scope. Currently set for
 * owned values and literals.
 * - **REFERENCE** The value can be taken address of (MIR), meaning reference of or pointer of (HOUT).
 */
MAKE_FLAG_TYPE(compiler::tsh, ValueSemanticsOptions, ValueSemantics,
	MOVE,
	COPY,
	REINIT,
	USE,
	REFERENCE,
	DESTROY
)

namespace compiler::tsh {
	/**
	 * @brief Describes properties of a value that are independent of its type. Like:
	 * - Where it came from - PrimaryCategory
	 * - What may/must be done with it - ValueSemantics.
	 */
	class ValueCategory {
		using enum ValueSemanticsOptions;

		/**
		 * Primary category of a value.
		 */
		PrimaryCategory category;
		/**
		 * If a value is pure it is guaranteed to not be changed behind the scenes.
		 */
		bool is_pure{ false };
		/**
		 * Allowed semantics describe what can be done with a value.
		 */
		ValueSemantics allows_semantic{ MOVE | COPY | REINIT | USE | DESTROY };
		/**
		 * Forced semantics describe what must be done with a value. Currently only the `MOVE` flag
		 * is used, others may be redundant.
		 */
		ValueSemantics force_semantic{};

	public:
		// It is not obvious what the default value category should be,
		// so the default constructor is disabled.
		ValueCategory() = delete;

		explicit ValueCategory(const PrimaryCategory&);

		ValueCategory(
			PrimaryCategory category,
			bool            is_pure,
			ValueSemantics  allows_semantic,
			ValueSemantics  force_semantic
		);

		[[nodiscard]]
		ValueCategory withForceSemantics(const ValueSemantics new_force_semantic) const {
			return { category, is_pure, allows_semantic, new_force_semantic };
		}

		[[nodiscard]]
		ValueCategory withDisabled(ValueSemantics disabled) const {
			auto allows_semantic_copy = allows_semantic;
			allows_semantic_copy -= disabled;
			auto force_semantic_copy = force_semantic;
			force_semantic_copy -= disabled;
			return { category, is_pure, allows_semantic_copy, force_semantic_copy };
		}

		/**
		 * A simple getter for category.
		 */
		[[nodiscard]]
		PrimaryCategory getCategory() const {
			return category;
		}

		/**
		 * A simple getter for is_pure.
		 */
		[[nodiscard]]
		bool isPure() const {
			return is_pure;
		}

		/**
		 * A simple getter for allows_semantic.
		 */
		[[nodiscard]]
		ValueSemantics getAllowsSemantic() const {
			return allows_semantic;
		}

		/**
		 * A simple getter for force_semantic.
		 */
		[[nodiscard]]
		ValueSemantics getForceSemantic() const {
			return force_semantic;
		}

		/**
		 * @brief Whether the value is an addressable location that may be re-assigned - an lvalue.
		 *
		 * True for Locals, Globals and Dereferenced.
		 */
		[[nodiscard]]
		bool canBeAssignedTo() const {
			return allows_semantic.contains(REINIT);
		}

		/**
		 * @brief Whether a reference to the value may be formed, i.e. it has an address.
		 * Implemented separately from `canBeAssignedTo` since those two may diverge in the future
		 * (for example allowing const reference to temporaries).
		 */
		[[nodiscard]]
		bool addressable() const {
			return allows_semantic.contains(REFERENCE);
		}

		/**
		 * @brief Whether the value is a valid operand of the explicit `move` operator.
		 *
		 * An owned lvalue, i.e. a Local.
		 */
		[[nodiscard]]
		bool isMovableFrom() const {
			return allows_semantic.contains(MOVE);
		}

		/**
		 * @brief Whether the value must be moved (e.g. the explicit `move` operator forces it).
		 */
		[[nodiscard]]
		bool mustMove() const {
			return force_semantic.contains(MOVE);
		}

		/**
		 * Compares value categories in terms of what might be done with values they describe.
		 * @param other Value category to compare
		 * @return Whether the other value category is contained in this value category.
		 */
		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			// `this` must allow everything `other` allows, and force no more than `other` forces.
			return allows_semantic.contains(other.allows_semantic)
			    && other.force_semantic.contains(force_semantic) && (!is_pure || other.is_pure);
		}

		auto operator<=>(const ValueCategory& other) const = default;

		/**
		 * @brief A human readable description of the value category.
		 */
		[[nodiscard]] std::string toString() const;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				category, is_pure, allows_semantic, force_semantic
			);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const ValueCategory& vc
		) noexcept {
			addToHash(h, vc.queryUnstablePerfectHash());
		}
	};
}
