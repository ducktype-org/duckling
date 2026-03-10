#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>

MAKE_STRINGIFYABLE_ENUM(compiler::helios, int, SymbolKind
		/**
        * @brief Stores general kind/type of symbol.
 		*/,
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
		Destructor
		// ...
)
