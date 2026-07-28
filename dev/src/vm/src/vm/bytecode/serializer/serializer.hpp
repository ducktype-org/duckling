#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <functional>
#include <iostream>

namespace vm::code {
	/**
	 * @brief Turns a symbol name into a human-readable annotation for it, or into an empty string
	 * when there is nothing worth annotating.
	 *
	 * The serializers emit the annotation as a `#` comment right above the declaration of the
	 * symbol. This is how the compiler makes de-mangled names of the symbols it generated show up
	 * in the emitted bytecode; the DVM itself never needs them (its lexer drops comments), so an
	 * empty provider simply produces no comments.
	 */
	using SymbolAnnotator = std::function<std::string(base::StrID symbol_name)>;

	/**
	 * @brief Serializes bytecode function into a parse-able by the DVM
	 * text representation.
	 */
	void serializeFunction(
		const Function& function, std::ostream& out, const SymbolAnnotator& annotate = {}
	);

	/**
	 * @brief Serializes bytecode type into a parse-able by the DVM
	 * text representation.
	 */
	void serializeType(
		const TypeOfData& type, std::ostream& out, const SymbolAnnotator& annotate = {}
	);

	/**
	 * @brief Serializes bytecode global data into a parse-able by the DVM
	 * text representation.
	 */
	void serializeGlobal(
		const GlobalData& type, std::ostream& out, const SymbolAnnotator& annotate = {}
	);

	/**
	 * @brief Serializes code collection into a parse-able by the DVM
	 * text representation.
	 */
	void serializeCode(
		const CodeCollection&  code_collection,
		std::ostream&          out,
		const SymbolAnnotator& annotate = {}
	);

	/**
	 * @brief Serializes constant value into a parse-able by the DVM
	 * text representation.
	 */
	void serializeConstValue(const ConstantValue& const_value, std::ostream& out);

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
