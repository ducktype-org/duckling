#include "value_category.hpp"

#include <helios/symbols/symbol_kind.hpp>

namespace compiler::tsh {
	PrimaryCategory primaryCategoryOfSymbol(compiler::helios::SymID symbol) {
		compiler::helios::SymbolKind symbol_kind = kind(symbol);
		// @TODO Properly check whether the symbol is local or global.
		bool is_symbol_local = symbol_kind == compiler::helios::SymbolKind::Variable;
		return is_symbol_local ? PrimaryCategory::Local : PrimaryCategory::Global;
	}

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
			is_pure         = true;
			allows_semantic = MOVE | COPY | USE | DESTROY;  // All but REINIT
			break;
		case PrimaryCategory::Local:
			category        = PrimaryCategory::Local;
			is_pure         = false;
			allows_semantic = MOVE | COPY | REINIT | USE | DESTROY;  // All
			break;
		case PrimaryCategory::Global:
			category        = PrimaryCategory::Global;
			is_pure         = false;
			allows_semantic = COPY | REINIT | USE;  // All but MOVE and DESTROY
			break;
		case PrimaryCategory::Literal:
			category        = PrimaryCategory::Literal;
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
		const bool            is_pure,
		const ValueSemantics  allows_semantic,
		const ValueSemantics  force_semantic
	):
		  category(category),
		  is_pure(is_pure),
		  allows_semantic(allows_semantic),
		  force_semantic(force_semantic) {}

}
