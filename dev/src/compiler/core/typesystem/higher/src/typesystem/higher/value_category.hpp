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

namespace compiler::tsh {
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
}

MAKE_FLAG_TYPE(compiler::tsh, ValueSemanticsOptions, ValueSemantics,
	MOVE,
	COPY,
	REINIT,
	USE,
	DESTROY
)

namespace compiler::tsh {
	/**
	 * Value category class describes properties of a value other than its type.
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
		 * Forces semantics describe what must be done with a value. Note that it might be redundant
		 * because of information kept in allowed semantics.
		 */
		ValueSemantics force_semantic{};

	public:
		// It is not obvious what the default value category should be,
		// so the default constructor is disabled.
		ValueCategory() = delete;

		ValueCategory(const ValueCategory&) = default;

		explicit ValueCategory(const PrimaryCategory&);

		ValueCategory(
			PrimaryCategory category,
			bool            is_pure,
			ValueSemantics  allows_semantic,
			ValueSemantics  force_semantic
		);

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
		 * Compares value categories in terms of what might be done with values they describe.
		 * @param other Value category to compare
		 * @return Whether the other value category is contained in this value category.
		 */
		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			return allows_semantic >= other.allows_semantic
			    && force_semantic <= other.force_semantic && (!is_pure || other.is_pure);
		}

		auto operator<=>(const ValueCategory& other) const = default;

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
