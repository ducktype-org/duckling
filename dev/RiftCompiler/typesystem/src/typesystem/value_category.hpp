/**
 * @file value_category.hpp
 * @brief Value category definition.
 *
 * Value category describes properties of the value that are not directly tied to its type.
 * Every TypeDesc has a ValueCategory object associated with it.
 */

#pragma once

#include <base/flag.hpp>
#include <helios/symbols/symbols.hpp>

namespace ts {
	// There used to be "Identifiable" category, but it is now replaced with "Local" and "Global"
	// Maybe in the future we want to bring back "Identifiable" and make a struct to keep more
	// information
	/**
	 * Primary category describes source of the value.
	 */
	enum class PrimaryCategory {
		Temporary, /**< Temporary values are product of expression evaluation. */
		Local,     /**< Local values correspond to local variables. */
		Global,    /**< Global values correspond to global variables. */
		Literal    /**< Literal values store values explicitly written in the code. */
	};

	/**
	 * @brief Get the primary category of a symbol: either Local or Global.
	 * @param symbol The symbol in question.
	 * @return The symbol's primary category.
	 */
	PrimaryCategory primaryCategoryOfSymbol(compiler::helios::SymID symbol);

	constexpr base::FlagType MOVE(0);
	constexpr base::FlagType COPY(1);
	constexpr base::FlagType REINIT(2);
	constexpr base::FlagType USE(3);
	constexpr base::FlagType DESTROY(4);

	/**
	 * Value category class describes properties of a value other than its type.
	 */
	class ValueCategory {
		/**
		 * Primary category of a value.
		 */
		PrimaryCategory category{ PrimaryCategory::Local };
		/**
		 * If value is mutable it can be implicitly changed by the coder.
		 */
		bool is_mutable{ false };
		/**
		 * If value is pure it is guaranteed to not be changed behind the scenes.
		 */
		bool is_pure{ false };
		/**
		 * Allowed semantics describe what can be done with a value.
		 */
		base::FlagType allows_semantic{ MOVE | COPY | REINIT | USE | DESTROY };
		/**
		 * Forces semantics describe what must be done with a value. Note that it might be redundant
		 * because of information kept in allowed semantics.
		 */
		base::FlagType force_semantic{};

	public:
		ValueCategory() = default;

		explicit ValueCategory(const PrimaryCategory&);

		ValueCategory(
			PrimaryCategory category,
			bool            is_mutable,
			bool            is_pure,
			base::FlagType  allows_semantic,
			base::FlagType  force_semantic
		);

		/**
		 * A simple getter for category.
		 */
		[[nodiscard]]
		PrimaryCategory getCategory() const {
			return category;
		}

		/**
		 * A simple getter for is_mutable.
		 */
		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
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
		base::FlagType getAllowsSemantic() const {
			return allows_semantic;
		}

		/**
		 * A simple getter for force_semantic.
		 */
		[[nodiscard]]
		base::FlagType getForceSemantic() const {
			return force_semantic;
		}

		/**
		 * Compares value categories in terms of what might be done with values they describe.
		 * @param other Value category to compare
		 * @return Whether the other value category is contained in this value category.
		 */
		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			return allows_semantic >= other.allows_semantic
			    && force_semantic <= other.force_semantic && (is_mutable || !other.is_mutable)
			    && (!is_pure || other.is_pure);
		}

		auto operator<=>(const ValueCategory& other) const = default;
	};
}
