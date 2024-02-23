/**
 * @file value_category.hpp
 * @brief Value category definition.
 */

#pragma once

#include <base/flag.hpp>

namespace ts {
	// There used to be "Identifiable" category, but it is now replaced with "Local" and "Global"
	// Maybe in the future we want to bring back "Identifiable" and make a struct to keed more
	// information
	enum class PrimaryCategory { Temporary, Local, Global, Literal };

	constexpr base::FlagType MOVE(0);
	constexpr base::FlagType COPY(1);
	constexpr base::FlagType REINIT(2);
	constexpr base::FlagType USE(3);
	constexpr base::FlagType DESTROY(4);

	class ValueCategory {
		PrimaryCategory category{ PrimaryCategory::Local };
		bool            is_mutable{ true };
		bool            is_pure{ false };
		base::FlagType  allows_semantic{ MOVE | COPY | REINIT | USE | DESTROY };
		base::FlagType  force_semantic{};

	public:
		ValueCategory() = default;

		explicit ValueCategory(const PrimaryCategory&);

		ValueCategory(PrimaryCategory, bool, bool, base::FlagType, base::FlagType);

		[[nodiscard]]
		PrimaryCategory getCategory() const {
			return category;
		}

		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
		}

		[[nodiscard]]
		bool isPure() const {
			return is_pure;
		}

		[[nodiscard]]
		base::FlagType getAllowsSemantic() const {
			return allows_semantic;
		}

		[[nodiscard]]
		base::FlagType getForceSemantic() const {
			return force_semantic;
		}

		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			return allows_semantic >= other.allows_semantic
			    && force_semantic <= other.force_semantic && (is_mutable || !other.is_mutable)
			    && (!is_pure || other.is_pure);
		}

		auto operator<=>(const ValueCategory& other) const = default;
	};
}
