#pragma once

#include "base/exceptions.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <iostream>

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

	/**
	 * @brief Serializes bytecode global data into a parse-able by the DVM
	 * text representation.
	 */
	void serialize(const GlobalData& type, std::ostream& out);
}
