#pragma once

#include <iostream>
#include <vm/program/program.hpp>

namespace vm::program {
	/**
	 * @brief Serializes bytecode CodeFile object into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const CodeFile& file, std::ostream& out);
}
