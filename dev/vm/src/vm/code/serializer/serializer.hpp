#pragma once

#include <iostream>
#include <vm/code/code.hpp>

namespace vm::code {
	/**
	 * @brief Serializes bytecode CodeFile object into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const CodeFile& file, std::ostream& out);
}
