#include "dvm_symbol_annotator.hpp"

#include <helios/mangler/demangler.hpp>

#include <string>

namespace compiler::driver {

	const vm::code::SymbolAnnotator& demangledSymbolAnnotator() {
		static const vm::code::SymbolAnnotator annotator = [](const base::StrID symbol_name) {
			return helios::mangler::tryDemangle(symbol_name.strView()).copyValueOr(std::string{});
		};
		return annotator;
	}
}
