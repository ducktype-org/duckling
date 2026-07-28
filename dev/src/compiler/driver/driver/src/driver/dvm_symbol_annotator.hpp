#pragma once

#include <vm/bytecode/serializer/serializer.hpp>

namespace compiler::driver {

	/**
	 * @brief The annotator handed to the DVM serializers, so that every declaration in the emitted
	 * bytecode is preceded by a comment holding the de-mangled name of its symbol.
	 *
	 * Names the compiler did not mangle (C linkage, DVM-internal helpers, ...) are left without a
	 * comment, and so are names this compiler cannot de-mangle - see mangler::tryDemangle().
	 */
	const vm::code::SymbolAnnotator& demangledSymbolAnnotator();
}
