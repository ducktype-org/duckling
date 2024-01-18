/**
 * @file value_category.hpp
 * @brief Value category definition
 */

#pragma once

#include <base/flag.hpp>

namespace ts {
	enum class PrimaryCategory { Identifiable, Temporary, Local, Global, Literal };

	constexpr base::FlagType MOVE(0);
	constexpr base::FlagType COPY(1);
	constexpr base::FlagType FORWARD(2);
	constexpr base::FlagType REINIT(3);

	class ValueCategory {
		PrimaryCategory category{ PrimaryCategory::Identifiable };
		bool            is_mutable{ true };
		bool 			is_pure { false };
		base::FlagType  allows_semantic{ MOVE | COPY | FORWARD | REINIT };
		base::FlagType  force_semantic{};

	public:
		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			return allows_semantic >= other.allows_semantic
			    && force_semantic <= other.force_semantic
				&& (is_mutable || !other.is_mutable)
				&& (!is_pure || other.is_pure);
		}

		auto operator<=>(const ValueCategory& other) const = default;
	};
}
