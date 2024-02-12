/**
 * @file kind.hpp
 * @brief Type kind definition
 */

#pragma once
#include <base/exceptions.hpp>
#include <sstream>

namespace ts {
	/**
	 * \brief Enum which identifies the features of a type described in the Type System.
	 *
	 * For example, the Void type does not hold much information about itself.
	 * However, each of the several Integral types holds a signedness boolean.
	 * Furthermore, a Tuple type holds information about its component types.
	 * Each class of types is described with a different Kind.
	 */
	enum class Kind : int32_t {
		Unit,
		Void,
		Byte,
		Bool,
		Char,
		Integral,
		Float,
		RawPointer,
		Pointer,
		Function,
		Enum,
		Flag,
		Optional,
		Tuple,
		Variant,
		Class,
		TypeTemplate,
		Namespace,
		CodeBlock,
		Module,
		VTable,

		/**
		 * \brief The kind of the type which holds type values. In other words, the "type" type.
		 */
		Meta,

		/**
		 * \brief The kind of the general TypeInfo(Impl). Must not be used in constructors or non-static contexts.
		 */
		Any = -1
	};

	inline std::string kindToString(Kind kind) {
		using enum Kind;
		switch(kind) {
		case Unit: return "Unit";
		case Void: return "Void";
		case Byte: return "Byte";
		case Bool: return "Bool";
		case Char: return "Char";
		case Integral: return "Integral";
		case Float: return "Float";
		case RawPointer: return "RawPointer";
		case Pointer: return "Pointer";
		case Function: return "Function";
		case Enum: return "Enum";
		case Flag: return "Flag";
		case Optional: return "Optional";
		case Tuple: return "Tuple";
		case Variant: return "Variant";
		case Class: return "Class";
		case TypeTemplate: return "TypeTemplate";
		case Namespace: return "Namespace";
		case CodeBlock: return "CodeBlock";
		case Module: return "Module";
		case Meta: return "Meta";
		case VTable: return "VTable";
		case Any: return "Any";
		default:
			std::stringstream ss;
			ss << "Tried to translate non-existent Kind with underlying value " << static_cast<int>(kind) << " to string.";
			RIFT_PANIC(ss.str());
		}
	}
}
