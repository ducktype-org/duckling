#pragma once

namespace compiler::helios {

	/**
	 * @brief Stores general kind/type of a symbol.
	 */
	enum class SymbolKind {
		Namespace,
		Function,
		Const,
		Class,
		Alias,
		Using,
		Variable,
		Import,
		Parameter,

		// we distinguish between functions and builtin functions
		// as for example there is no code-gen for builtin functions
		BuiltinFunction,

		// Class Symbols
		Method,
		Field,
		Constructor,
		Destructor,
		// ...
	};
}
