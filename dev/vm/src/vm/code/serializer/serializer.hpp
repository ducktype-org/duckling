#pragma once

#include "vm/code/type_of_data.hpp"
#include <iostream>
#include <vm/code/code.hpp>

namespace vm::code {
	/**
	 * @brief Serializes bytecode function into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const Function& function, std::ostream& out);

	/**
	 * @brief Serializes bytecode type into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const TypeOfData& type, std::ostream& out);
}
