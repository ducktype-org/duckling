#include "value_category.hpp"

namespace ts {
	/**
	 * @brief Construct the value category with default attributes based on primary category.
	 * @param pc The primary category.
	 */
	ValueCategory::ValueCategory(const PrimaryCategory& pc) {
		// @TODO
		// Default values might need some tweaking in the future
		switch (pc) {
		case PrimaryCategory::Temporary:
			category        = PrimaryCategory::Temporary;
			is_mutable      = false;
			is_pure         = true;
			allows_semantic = MOVE | COPY | USE | DESTROY;  // All but REINIT
			break;
		case PrimaryCategory::Local:
			category        = PrimaryCategory::Local;
			is_mutable      = true;
			is_pure         = false;
			allows_semantic = MOVE | COPY | REINIT | USE | DESTROY;  // All
			break;
		case PrimaryCategory::Global:
			category        = PrimaryCategory::Global;
			is_mutable      = true;
			is_pure         = false;
			allows_semantic = COPY | REINIT | USE;  // All but MOVE and DESTROY
			break;
		case PrimaryCategory::Literal:
			category        = PrimaryCategory::Literal;
			is_mutable      = false;
			is_pure         = true;
			allows_semantic = COPY | USE | DESTROY;  // All but MOVE and REINIT
			break;
		}
	}

	/**
	 * @brief Construct the value category with manually given values of all attributes.
	 */
	ValueCategory::ValueCategory(
		const PrimaryCategory category,
		const bool            is_mutable,
		const bool            is_pure,
		const base::FlagType  allows_semantic,
		const base::FlagType  force_semantic
	):
		  category(category),
		  is_mutable(is_mutable),
		  is_pure(is_pure),
		  allows_semantic(allows_semantic),
		  force_semantic(force_semantic) {}

}
