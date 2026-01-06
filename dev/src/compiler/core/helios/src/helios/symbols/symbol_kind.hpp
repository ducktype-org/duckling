#pragma once

namespace compiler::helios {

	/**
	 * @brief Stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Namespace,
		Function,
		FunctionDeclaration,
		Const,
		Class,
		Alias,
		Using,
		Variable,
		Import,
		Parameter,

		// Class-specific Symbols
		Method,
		Field,
		Constructor,
		Destructor,

		// ...
	};
}
