/**
 * @file value_category.hpp
 * @brief Value category definition.
 */

#pragma once

#include <base/flag.hpp>

namespace ts {
	// There used to be "Identifiable" category, but it is now replaced with "Local" and "Global"
	// Maybe in the future we want to bring back "Identifiable" and make a struct to keed more information
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
		static ValueCategory create() {
			ValueCategory result;
			return result;
		}

		static ValueCategory create(const PrimaryCategory& pc) {
			ValueCategory result;
			// @TODO
			// Default values might need some tweaking in the future
			switch(pc) {
				case PrimaryCategory::Temporary:
					result.category = PrimaryCategory::Temporary;
					result.is_mutable = false;
					result.is_pure = true;
					result.allows_semantic = MOVE | COPY | USE | DESTROY; // All but REINIT
					break;
				case PrimaryCategory::Local:
					result.category = PrimaryCategory::Local;
					result.is_mutable = true;
					result.is_pure = false;
					result.allows_semantic = MOVE | COPY | REINIT | USE | DESTROY; // All
					break;
				case PrimaryCategory::Global:
					result.category = PrimaryCategory::Global;
					result.is_mutable = true;
					result.is_pure = false;
					result.allows_semantic = COPY | REINIT | USE ; // All but MOVE and DESTROY
					break;
				case PrimaryCategory::Literal:
					result.category = PrimaryCategory::Literal;
					result.is_mutable = false;
					result.is_pure = true;
					result.allows_semantic = COPY | USE | DESTROY; // All but MOVE and REINIT
					break;
			}
			return result;
		}

		PrimaryCategory getCategory() {
			return category;
		}

		bool isMutable() {
			return is_mutable;
		}

		bool isPure() {
			return is_pure;
		}

		base::FlagType getAllowsSemantic() {
			return allows_semantic;
		}

		base::FlagType getForceSemantic() {
			return force_semantic;
		}

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
