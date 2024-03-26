/**
 * @file value_category.hpp
 * @brief Value category definition
 */

#pragma once

#include <base/flag.hpp>

namespace ts {
	enum class PrimaryCategory { Identifiable, Temporary, Literal };

	constexpr base::FlagType MOVE(0);
	constexpr base::FlagType COPY(1);
	constexpr base::FlagType FORWARD(2);
	constexpr base::FlagType REINIT(3);

	class ValueCategory {
		PrimaryCategory category{ PrimaryCategory::Identifiable };
		bool            is_const{ false };
		base::FlagType  allows_semantic{ MOVE | COPY | FORWARD | REINIT };
		base::FlagType  force_semantic{};

	public:
		[[nodiscard]]
		bool contains(const ValueCategory& other) const {
			return allows_semantic >= other.allows_semantic
			    && force_semantic <= other.force_semantic && (!is_const || other.is_const);
		}

		auto operator<=>(const ValueCategory& other) const = default;
	};
}
