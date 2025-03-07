#pragma once

#include <iostream>
#include "elements.hpp"

namespace compiler::backend_vm {
	/**
	 * @brief Serializes bytecode CodeFile object into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const CodeFile& file, std::ostream& out);
}
