#pragma once
#include <json/json.hpp>

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

	NLOHMANN_JSON_SERIALIZE_ENUM(
		SymbolKind,
		{ { SymbolKind::Namespace, "namespace" },
	      { SymbolKind::Function, "function" },
	      { SymbolKind::Const, "const" },
	      { SymbolKind::Class, "class" },
	      { SymbolKind::Alias, "alias" },
	      { SymbolKind::Using, "using" },
	      { SymbolKind::Variable, "variable" },
	      { SymbolKind::Import, "import" },
	      { SymbolKind::Parameter, "parameter" },
	      { SymbolKind::BuiltinFunction, "builtin_function" },
	      { SymbolKind::Method, "method" },
	      { SymbolKind::Field, "field" },
	      { SymbolKind::Constructor, "constructor" },
	      { SymbolKind::Destructor, "destructor" } }
	)

}
