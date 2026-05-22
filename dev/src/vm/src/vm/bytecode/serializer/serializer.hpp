#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <iostream>

namespace vm::code {
	/**
	 * @brief Serializes bytecode function into a parse-able by the DVM
	 * text representation.
	 */
	void serializeFunction(const Function& function, std::ostream& out);

	/**
	 * @brief Serializes bytecode type into a parse-able by the DVM
	 * text representation.
	 */
	void serializeType(const TypeOfData& type, std::ostream& out);

	/**
	 * @brief Serializes bytecode global data into a parse-able by the DVM
	 * text representation.
	 */
	void serializeGlobal(const GlobalData& type, std::ostream& out);

	/**
	 * @brief Serializes code collection into a parse-able by the DVM
	 * text representation.
	 */
	void serializeCode(const CodeCollection& code_collection, std::ostream& out);

	/**
	 * @brief Stringifies instruction arguments.
	 */
	std::string argumentToString(const opargs::OpCodeArg& arg);

	/**
	 * @brief Stringifies instruction arguments.
	 */
	std::string argumentToString(opargs::OpCodeArgCRef arg);

	/**
	 * @brief Stringifies an instruction.
	 */
	std::string instructionToString(const Instruction& instruction);

	/**
	 * @brief Stringifies a type.
	 */
	std::string typeToString(const TypeOfData& type);
}
